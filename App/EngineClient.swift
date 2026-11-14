import Foundation
import CaptureEngine

enum EngineState: Equatable { case idle, stopped, connecting, waitingOBS, streaming, retrying, pinRequired, failed, unavailable }
struct EngineStatus: Equatable {
    var message = ""
    var width: UInt32 = 0, height: UInt32 = 0, fpsNumerator: UInt32 = 0, fpsDenominator: UInt32 = 1
    var videoPackets: UInt64 = 0, audioPackets: UInt64 = 0, reconnects: UInt64 = 0, queuedBytes: UInt64 = 0
    var lostVideoFrames: UInt64 = 0, missingAudioPackets: UInt64 = 0, queueAgeMicroseconds: UInt64 = 0
    var outputDisconnects: UInt64 = 0
}
@MainActor
protocol EngineControlling: AnyObject {
    var onState: ((EngineState) -> Void)? { get set }
    func start() -> Bool
    func configure(options: StreamOptions, account: Account, console: PairedConsole) -> Bool
    func status() -> EngineStatus
    func submitPIN(_ pin: String) -> Bool
    func stop()
    func shutdown() async
}
extension EngineControlling {
    func configure(options: StreamOptions, account: Account, console: PairedConsole) -> Bool { false }
    func status() -> EngineStatus { EngineStatus() }
    func submitPIN(_ pin: String) -> Bool { false }
}

private final class EngineRelay: @unchecked Sendable {
    @MainActor var handler: ((EngineState) -> Void)?
    func receive(_ state: EngineState) {
        Task { @MainActor [weak self] in self?.handler?(state) }
    }
}

// Ownership is transferred exactly once from client deinit to a cleanup worker.
// No client accesses the handle afterward; context outlives the native join.
private struct EngineTeardown: @unchecked Sendable {
    let handle: OpaquePointer
    let context: EngineRelay
    func run() {
        _ = capture_engine_destroy(handle)
        withExtendedLifetime(context) {}
    }
}

@MainActor
final class NativeEngineClient: EngineControlling {
    nonisolated static func decodedStatus(_ value: capture_status) -> EngineStatus {
        let message = withUnsafeBytes(of: value.message) { String(decoding: $0.prefix(while: { $0 != 0 }), as: UTF8.self) }
        return EngineStatus(message: message, width: value.width, height: value.height, fpsNumerator: value.fps_num,
                            fpsDenominator: value.fps_den, videoPackets: value.video_packets, audioPackets: value.audio_packets,
                            reconnects: value.reconnects, queuedBytes: value.queued_bytes,
                            lostVideoFrames: value.lost_video_frames, missingAudioPackets: value.missing_audio_packets,
                            queueAgeMicroseconds: value.queue_age_us, outputDisconnects: value.output_disconnects)
    }
    private let relay = EngineRelay()
    private var handle: OpaquePointer?
    var onState: ((EngineState) -> Void)? {
        get { relay.handler }
        set { relay.handler = newValue }
    }
    init() {
        var options = capture_default_options()
        var error = CAPTURE_OK
        handle = capture_engine_create(&options, { state, context in
            guard let context else { return }
            let relay = Unmanaged<EngineRelay>.fromOpaque(context).takeUnretainedValue()
            let mapped: EngineState
            switch state {
            case CAPTURE_STOPPED: mapped = .stopped
            case CAPTURE_CONNECTING: mapped = .connecting
            case CAPTURE_WAITING_OBS: mapped = .waitingOBS
            case CAPTURE_STREAMING: mapped = .streaming
            case CAPTURE_RETRYING: mapped = .retrying
            case CAPTURE_PIN_REQUIRED: mapped = .pinRequired
            case CAPTURE_FAILED: mapped = .failed
            default: mapped = .idle
            }
            relay.receive(mapped)
        }, Unmanaged.passUnretained(relay).toOpaque(), &error)
    }
    func start() -> Bool {
        guard let handle else { return false }
        return capture_engine_start(handle) == CAPTURE_OK
    }
    func configure(options: StreamOptions, account: Account, console: PairedConsole) -> Bool {
        guard let handle, options.isValid, (try? console.validate(for: account)) != nil else { return false }
        var nativeOptions = options.native
        var connection = capture_connection()
        connection.size = UInt32(MemoryLayout.size(ofValue: connection)); connection.version = UInt32(CAPTURE_API_VERSION)
        withUnsafeMutableBytes(of: &connection.address) { $0.copyBytes(from: console.address.utf8) }
        withUnsafeMutableBytes(of: &connection.account_id) { $0.copyBytes(from: account.accountID) }
        connection.registration.target = console.target; connection.registration.key_type = console.keyType
        withUnsafeMutableBytes(of: &connection.registration.mac) { $0.copyBytes(from: console.mac) }
        withUnsafeMutableBytes(of: &connection.registration.registration_key) { $0.copyBytes(from: console.registrationKey) }
        withUnsafeMutableBytes(of: &connection.registration.session_key) { $0.copyBytes(from: console.sessionKey) }
        defer { withUnsafeMutableBytes(of: &connection) { $0.initializeMemory(as: UInt8.self, repeating: 0) } }
        return capture_engine_configure(handle, &nativeOptions, &connection) == CAPTURE_OK
    }
    func status() -> EngineStatus {
        guard let handle else { return EngineStatus() }
        var value = capture_status()
        value.size = UInt32(MemoryLayout.size(ofValue: value)); value.version = UInt32(CAPTURE_API_VERSION)
        guard capture_engine_get_status(handle, &value) == CAPTURE_OK else { return EngineStatus() }
        return Self.decodedStatus(value)
    }
    func submitPIN(_ pin: String) -> Bool {
        guard let handle, pin.utf8.count == 4, pin.utf8.allSatisfy({ (48...57).contains($0) }) else { return false }
        return pin.withCString { capture_engine_submit_login_pin(handle, $0, 4) == CAPTURE_OK }
    }
    func stop() {
        if let handle { capture_engine_stop(handle) }
    }
    func shutdown() async {
        relay.handler = nil
        guard let handle else { return }
        self.handle = nil
        let teardown = EngineTeardown(handle: handle, context: relay)
        await withCheckedContinuation { continuation in
            DispatchQueue.global(qos: .utility).async {
                teardown.run()
                continuation.resume()
            }
        }
    }
    deinit {
        guard let handle else { return }
        // Hold callback context until the worker has joined; never block UI
        // teardown or ask a worker callback to wait on the main actor.
        let teardown = EngineTeardown(handle: handle, context: relay)
        DispatchQueue.global(qos: .utility).async { teardown.run() }
    }
}
