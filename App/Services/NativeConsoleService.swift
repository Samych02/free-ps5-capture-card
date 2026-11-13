import Foundation
import CaptureEngine

private func nativeText<T>(_ value: T) -> String {
    withUnsafeBytes(of: value) { String(decoding: $0.prefix(while: { $0 != 0 }), as: UTF8.self) }
}
private final class SetupRelay<Value>: @unchecked Sendable {
    @MainActor var handler: ((Value) -> Void)?
    func post(_ value: Value) { Task { @MainActor [self] in handler?(value) } }
}
private final class PairRelay: @unchecked Sendable {
    let request: PairingRequest
    let events = SetupRelay<Result<PairedConsole, PairingError>>()
    init(_ request: PairingRequest) { self.request = request }
}
private struct SetupTeardown: @unchecked Sendable {
    let handle: OpaquePointer
    let context: AnyObject
    let pairing: Bool
    var destroyPairing: (OpaquePointer?) -> capture_result = capture_pairing_destroy
    func run() {
        if pairing { _ = destroyPairing(handle) } else { _ = capture_discovery_destroy(handle) }
        withExtendedLifetime(context) {}
    }
    func wait() async {
        await withCheckedContinuation { continuation in
            DispatchQueue.global(qos: .utility).async { run(); continuation.resume() }
        }
    }
}
struct NativePairingAPI {
    var create = capture_pairing_create
    var stop = capture_pairing_stop
    var destroy = capture_pairing_destroy
}
@MainActor final class NativeConsoleService: ConsoleServicing {
    private let pairingAPI: NativePairingAPI
    init(pairingAPI: NativePairingAPI = NativePairingAPI()) { self.pairingAPI = pairingAPI }
    private var discoveryHandle: OpaquePointer?
    private var discoveryRelay: SetupRelay<Result<[DiscoveredConsole], PairingError>>?
    private var pairingHandle: OpaquePointer?
    private var pairingRelay: PairRelay?
    private var pairingCleanup: Task<Void, Never>?
    private var discoveryCleanup: Task<Void, Never>?
    private var shuttingDown = false
    nonisolated static func options(_ request: PairingRequest) -> capture_pairing_options {
        var value = capture_pairing_options()
        value.size = UInt32(MemoryLayout.size(ofValue: value)); value.version = UInt32(CAPTURE_API_VERSION)
        value.target = 1000100
        withUnsafeMutableBytes(of: &value.address) { $0.copyBytes(from: request.address.utf8) }
        withUnsafeMutableBytes(of: &value.pin) { $0.copyBytes(from: request.pin.utf8) }
        withUnsafeMutableBytes(of: &value.account_id) { $0.copyBytes(from: request.account.accountID) }
        return value
    }
    nonisolated static func registration(_ value: capture_registration, request: PairingRequest) -> PairedConsole {
        PairedConsole(accountID: request.account.accountID, address: request.address,
                      mac: withUnsafeBytes(of: value.mac) { Data($0) }, name: nativeText(value.name), target: value.target,
                      registrationKey: withUnsafeBytes(of: value.registration_key) { Data($0) },
                      sessionKey: withUnsafeBytes(of: value.session_key) { Data($0) }, keyType: value.key_type)
    }
    func discover(_ handler: @escaping (Result<[DiscoveredConsole], PairingError>) -> Void) {
        guard !shuttingDown, discoveryCleanup == nil else { return }
        if let discoveryRelay { discoveryRelay.handler = handler; return }
        let relay = SetupRelay<Result<[DiscoveredConsole], PairingError>>()
        relay.handler = handler; discoveryRelay = relay
        var error = CAPTURE_OK
        discoveryHandle = capture_discovery_create({ status, hosts, count, context in
            guard let context else { return }
            let relay = Unmanaged<SetupRelay<Result<[DiscoveredConsole], PairingError>>>.fromOpaque(context).takeUnretainedValue()
            if status != CAPTURE_SETUP_OK {
                relay.post(.failure(status == CAPTURE_SETUP_PERMISSION_DENIED ? .permission : .failed)); return
            }
            let consoles = (0..<count).compactMap { i -> DiscoveredConsole? in
                guard let value = hosts?[i] else { return nil }
                return DiscoveredConsole(id: nativeText(value.id).lowercased(), address: nativeText(value.address),
                                         name: nativeText(value.name), target: value.target, standby: value.state == 2)
            }
            relay.post(.success(consoles))
        }, Unmanaged.passUnretained(relay).toOpaque(), &error)
        if discoveryHandle == nil { discoveryRelay = nil; handler(.failure(.failed)) }
    }
    func stopDiscovery() async {
        discoveryRelay?.handler = nil
        guard let handle = discoveryHandle, let relay = discoveryRelay else {
            await discoveryCleanup?.value
            return
        }
        discoveryHandle = nil; discoveryRelay = nil
        capture_discovery_stop(handle)
        let teardown = SetupTeardown(handle: handle, context: relay, pairing: false)
        let cleanup = Task { await teardown.wait() }
        discoveryCleanup = cleanup
        await cleanup.value
        discoveryCleanup = nil
    }
    func pair(_ request: PairingRequest) async throws -> PairedConsole {
        guard pairingRelay == nil, pairingCleanup == nil, !shuttingDown, !Task.isCancelled else { throw PairingError.cancelled }
        let relay = PairRelay(request); pairingRelay = relay
        let result: Result<PairedConsole, PairingError> = await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                relay.events.handler = { continuation.resume(returning: $0) }
                var options = Self.options(request); var error = CAPTURE_OK
                pairingHandle = pairingAPI.create(&options, { status, registration, context in
                    guard let context else { return }
                    let relay = Unmanaged<PairRelay>.fromOpaque(context).takeUnretainedValue()
                    if status == CAPTURE_SETUP_OK, let registration {
                        relay.events.post(.success(NativeConsoleService.registration(registration.pointee, request: relay.request)))
                    } else { relay.events.post(.failure(status == CAPTURE_SETUP_CANCELLED ? .cancelled : .failed)) }
                }, Unmanaged.passUnretained(relay).toOpaque(), &error)
                if pairingHandle == nil { relay.events.handler = nil; continuation.resume(returning: .failure(.failed)) }
                else if Task.isCancelled { cancelPairing() }
            }
        } onCancel: {
            Task { @MainActor [weak self] in
                if self?.pairingRelay === relay { self?.cancelPairing() }
            }
        }
        relay.events.handler = nil
        if let handle = pairingHandle {
            pairingHandle = nil
            let teardown = SetupTeardown(handle: handle, context: relay, pairing: true, destroyPairing: pairingAPI.destroy)
            pairingCleanup = Task { await teardown.wait() }
        }
        await pairingCleanup?.value
        pairingCleanup = nil
        if pairingRelay === relay { pairingRelay = nil }
        guard !Task.isCancelled else { throw PairingError.cancelled }
        return try result.get()
    }
    func cancelPairing() { if let pairingHandle { pairingAPI.stop(pairingHandle) } }
    func shutdown() async {
        shuttingDown = true; cancelPairing()
        await stopDiscovery()
        if let handle = pairingHandle, let relay = pairingRelay {
            pairingHandle = nil
            let teardown = SetupTeardown(handle: handle, context: relay, pairing: true, destroyPairing: pairingAPI.destroy)
            pairingCleanup = Task { await teardown.wait() }
        }
        await pairingCleanup?.value
    }
    deinit {
        if let handle = discoveryHandle, let context = discoveryRelay {
            let cleanup = SetupTeardown(handle: handle, context: context, pairing: false)
            DispatchQueue.global(qos: .utility).async { cleanup.run() }
        }
        if let handle = pairingHandle, let context = pairingRelay {
            let cleanup = SetupTeardown(handle: handle, context: context, pairing: true, destroyPairing: pairingAPI.destroy)
            DispatchQueue.global(qos: .utility).async { cleanup.run() }
        }
    }
}
