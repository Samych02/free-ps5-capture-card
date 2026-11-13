import Combine
import Foundation

@MainActor
final class AppModel: ObservableObject {
    typealias State = EngineState
    @Published private(set) var state: State = .idle
    @Published private(set) var status = EngineStatus()
    @Published private(set) var active = false
    @Published var options: StreamOptions { didSet { if options.isValid { options.save(to: defaults) } } }
    private let defaults: UserDefaults
    private let engine: EngineControlling
    init(engine: EngineControlling? = nil, defaults: UserDefaults = .standard) {
        self.defaults = defaults
        options = StreamOptions.load(from: defaults)
        self.engine = engine ?? NativeEngineClient()
        self.engine.onState = { [weak self] state in
            guard let self else { return }
            switch state {
            case .idle: break // Do not overwrite a later error with startup idle.
            case .stopped: self.state = .stopped; self.active = false
            default: self.state = state
            }
            self.status = self.engine.status()
        }
    }
    func requestStart() {
        if !engine.start() { state = .unavailable }
    }
    func requestStart(account: Account?, console: PairedConsole?) {
        guard !active, let account, let console,
              engine.configure(options: options, account: account, console: console), engine.start() else {
            state = .unavailable; return
        }
        active = true; state = .connecting
    }
    func submitPIN(_ value: String) -> Bool { engine.submitPIN(value) }
    var obsURL: String { "tcp://127.0.0.1:\(options.port)" }
    func stop() { engine.stop() }
    func shutdown() async {
        await engine.shutdown()
        state = .stopped
    }
}
