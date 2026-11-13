import Combine

@MainActor
final class AppModel: ObservableObject {
    enum State: Equatable { case idle, stopped, unavailable }
    @Published private(set) var state: State = .idle
    private let engine: EngineControlling
    init(engine: EngineControlling? = nil) {
        self.engine = engine ?? NativeEngineClient()
        self.engine.onState = { [weak self] state in
            guard let self else { return }
            switch state {
            case .idle: break // Do not overwrite a later error with startup idle.
            case .stopped: self.state = .stopped
            }
        }
    }
    func requestStart() {
        if !engine.start() { state = .unavailable }
    }
    func stop() { engine.stop() }
    func shutdown() async {
        await engine.shutdown()
        state = .stopped
    }
}
