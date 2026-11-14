import Combine
import Foundation

@MainActor
final class AppModel: ObservableObject {
    typealias State = EngineState
    @Published private(set) var state: State = .idle
    @Published private(set) var status = EngineStatus()
    @Published private(set) var active = false
    @Published private(set) var isStopping = false
    @Published private(set) var isSubmittingPIN = false
    @Published private(set) var pinSubmissionError: String?
    @Published var portText: String {
        didSet {
            if let port = StreamOptions.validPort(portText), port != options.port { options.port = port }
        }
    }
    @Published var showOutputMetrics: Bool {
        didSet { defaults.set(showOutputMetrics, forKey: "showOutputMetrics") }
    }
    @Published var options: StreamOptions {
        didSet {
            if options.port != oldValue.port, StreamOptions.validPort(portText) != options.port { portText = String(options.port) }
            if options.isValid { options.save(to: defaults) }
        }
    }
    private let defaults: UserDefaults
    private let engine: EngineControlling
    init(engine: EngineControlling? = nil, defaults: UserDefaults = .standard) {
        self.defaults = defaults
        showOutputMetrics = defaults.bool(forKey: "showOutputMetrics")
        let saved = StreamOptions.load(from: defaults)
        options = saved; portText = String(saved.port)
        self.engine = engine ?? NativeEngineClient()
        self.engine.onState = { [weak self] state in
            guard let self else { return }
            let snapshot = self.engine.status()
            let current = snapshot.nativeState ?? state
            if self.isStopping && current != .stopped { return }
            if current != .pinRequired || snapshot.message != self.status.message {
                self.isSubmittingPIN = false
                self.pinSubmissionError = nil
            }
            switch current {
            case .idle: break // Do not overwrite a later error with startup idle.
            case .stopped: self.state = .stopped; self.active = false; self.isStopping = false
            default: self.state = current
            }
            self.status = snapshot
        }
    }
    func requestStart() {
        guard !active, !isStopping else { return }
        guard optionsAreValid else { state = .unavailable; return }
        if engine.start() { active = true; state = .connecting } else { state = .unavailable }
    }
    func requestStart(account: Account?, console: PairedConsole?) {
        guard !active, !isStopping else { return }
        guard let account, let console,
              optionsAreValid, engine.configure(options: options, account: account, console: console), engine.start() else {
            state = .unavailable; return
        }
        active = true; state = .connecting
    }
    func submitPIN(_ value: String) -> Bool {
        guard state == .pinRequired, !isSubmittingPIN,
              value.utf8.count == 4, value.utf8.allSatisfy({ (48...57).contains($0) }) else { return false }
        guard engine.submitPIN(value) else {
            pinSubmissionError = "Could not send the passcode. Check the connection, or stop capture and try again."
            return false
        }
        pinSubmissionError = nil; isSubmittingPIN = true
        return true
    }
    func dismissPINSheet() {
        if state == .pinRequired && !isStopping { stop() }
    }
    func updateConsoleAddress(_ address: String, expected: PairedConsole, pairing: PairingModel) -> Bool {
        guard !active else { return false }
        return pairing.updateAddress(address, expected: expected)
    }
    var obsURL: String { "tcp://127.0.0.1:\(options.port)" }
    var optionsAreValid: Bool { options.isValid && StreamOptions.validPort(portText) == options.port }
    func stop() {
        guard !isStopping else { return }
        isStopping = true; isSubmittingPIN = false; pinSubmissionError = nil
        engine.stop()
    }
    func shutdown() async {
        await engine.shutdown()
        state = .stopped
        active = false; isStopping = false; isSubmittingPIN = false; pinSubmissionError = nil
    }
}
