import Foundation
import CaptureEngine

enum EngineState: Equatable { case idle, stopped }
@MainActor
protocol EngineControlling: AnyObject {
    var onState: ((EngineState) -> Void)? { get set }
    func start() -> Bool
    func stop()
    func shutdown() async
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
            relay.receive(state == CAPTURE_STOPPED ? .stopped : .idle)
        }, Unmanaged.passUnretained(relay).toOpaque(), &error)
    }
    func start() -> Bool {
        guard let handle else { return false }
        return capture_engine_start(handle) == CAPTURE_OK
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
