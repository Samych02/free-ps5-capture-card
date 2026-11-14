import XCTest
@testable import FreePS5CaptureCard

@MainActor
final class FakeEngine: EngineControlling {
    var onState: ((EngineState) -> Void)?
    var starts = 0
    var stops = 0
    var available = false
    var snapshot = EngineStatus()
    var acceptsPIN = false
    var submittedPINs = 0
    var acknowledgesStop = true
    func configure(options: StreamOptions, account: Account, console: PairedConsole) -> Bool { available }
    func start() -> Bool { starts += 1; return available }
    func stop() { stops += 1; if acknowledgesStop { onState?(.stopped) } }
    func shutdown() async { stop() }
    func status() -> EngineStatus { snapshot }
    func submitPIN(_ pin: String) -> Bool { submittedPINs += 1; return acceptsPIN }
}

final class AppModelTests: XCTestCase {
    @MainActor func testInvalidVisiblePortTextCannotStartWithPreviousValidPort() throws {
        let suite = "port-tests-\(UUID().uuidString)"
        let defaults = try XCTUnwrap(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let engine = FakeEngine(); engine.available = true
        let model = AppModel(engine: engine, defaults: defaults)
        model.portText = "abc"
        XCTAssertFalse(model.optionsAreValid)
        model.requestStart()
        XCTAssertEqual(engine.starts, 0)
        model.portText = "65536"; XCTAssertFalse(model.optionsAreValid)
        model.portText = "１２３４"; XCTAssertFalse(model.optionsAreValid)
        model.portText = "12345"
        XCTAssertTrue(model.optionsAreValid); XCTAssertEqual(model.options.port, 12345)
        XCTAssertEqual(model.obsURL, "tcp://127.0.0.1:12345")
    }
    @MainActor func testNativeSnapshotWinsOverAnOlderQueuedStateCallback() {
        let engine = FakeEngine()
        let subject = AppModel(engine: engine)
        engine.snapshot.nativeState = .streaming; engine.snapshot.phase = .streaming
        engine.onState?(.retrying)
        XCTAssertEqual(subject.state, .streaming)
    }
    @MainActor func testStopIgnoresLateStreamingUntilAcknowledgement() {
        let engine = FakeEngine(); engine.acknowledgesStop = false
        let subject = AppModel(engine: engine)
        engine.onState?(.streaming)
        subject.stop()
        XCTAssertTrue(subject.isStopping)
        engine.onState?(.streaming)
        XCTAssertTrue(subject.isStopping)
        subject.stop(); XCTAssertEqual(engine.stops, 1)
        engine.onState?(.stopped)
        XCTAssertFalse(subject.isStopping)
        XCTAssertEqual(subject.state, .stopped)
        XCTAssertFalse(subject.active)
    }
    @MainActor func testPINSubmissionRejectsInvalidInputWithoutRetainingSecret() {
        let engine = FakeEngine()
        let subject = AppModel(engine: engine); engine.onState?(.pinRequired)
        XCTAssertFalse(subject.submitPIN("12")); XCTAssertEqual(engine.submittedPINs, 0)
        XCTAssertFalse(subject.submitPIN("1234")); XCTAssertNotNil(subject.pinSubmissionError)
        engine.acceptsPIN = true
        XCTAssertTrue(subject.submitPIN("1234")); XCTAssertTrue(subject.isSubmittingPIN)
        XCTAssertNil(subject.pinSubmissionError)
        XCTAssertFalse(subject.submitPIN("1234")); XCTAssertEqual(engine.submittedPINs, 2)
        engine.onState?(.pinRequired)
        XCTAssertTrue(subject.isSubmittingPIN, "An unchanged periodic status is not a new PIN response")
        engine.snapshot.message = "PS5 login passcode was incorrect. Try again."
        engine.onState?(.pinRequired)
        XCTAssertFalse(subject.isSubmittingPIN)
        engine.onState?(.streaming)
        XCTAssertFalse(subject.isSubmittingPIN)
        XCTAssertNil(subject.pinSubmissionError)
    }
    @MainActor func testSuccessfulPINDismissalCannotStopRecoveredCapture() {
        let engine = FakeEngine()
        let model = AppModel(engine: engine)
        engine.onState?(.pinRequired); engine.onState?(.streaming)
        model.dismissPINSheet(); XCTAssertEqual(engine.stops, 0)
        engine.onState?(.pinRequired)
        model.dismissPINSheet(); XCTAssertEqual(engine.stops, 1)
        XCTAssertEqual(model.state, .stopped)
    }
    @MainActor func testOutputMetricsAreHiddenByDefault() throws {
        let suite = "metrics-tests-\(UUID().uuidString)"
        let defaults = try XCTUnwrap(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        XCTAssertFalse(AppModel(engine: FakeEngine(), defaults: defaults).showOutputMetrics)
    }
    @MainActor func testOutputMetricsVisibilityPersistsWithoutControllingCapture() throws {
        let suite = "metrics-tests-\(UUID().uuidString)"
        let defaults = try XCTUnwrap(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let engine = FakeEngine()
        let model = AppModel(engine: engine, defaults: defaults)
        model.showOutputMetrics = true
        XCTAssertTrue(AppModel(engine: FakeEngine(), defaults: defaults).showOutputMetrics)
        model.showOutputMetrics = false
        XCTAssertFalse(AppModel(engine: FakeEngine(), defaults: defaults).showOutputMetrics)
        XCTAssertEqual(engine.starts, 0)
        XCTAssertEqual(engine.stops, 0)
        XCTAssertEqual(model.state, .idle)
    }
    @MainActor func testAddressCanOnlyChangeAfterCaptureStops() throws {
        let account = try Account(decimalID: "42")
        let original = PairedConsole(accountID: account.accountID, address: "192.168.1.20", mac: Data(repeating: 1, count: 6), name: "PS5",
                                     target: 1000100, registrationKey: Data(repeating: 2, count: 16), sessionKey: Data(repeating: 3, count: 16), keyType: 0)
        let access = FakeCredentialFile(), engine = FakeEngine()
        let store = FileCredentialStore(access: access)
        try store.saveAccount(account); try store.savePairing(original)
        let pairing = PairingModel(service: FakeConsoleService(), store: store); pairing.setAccount(account)
        engine.available = true
        let model = AppModel(engine: engine)
        model.requestStart(account: account, console: original)
        let writes = access.writeAttempts
        XCTAssertFalse(model.updateConsoleAddress("192.168.1.35", expected: original, pairing: pairing))
        XCTAssertEqual(access.writeAttempts, writes); XCTAssertEqual(pairing.paired, original)
        XCTAssertEqual(model.state, .connecting)
        model.stop()
        XCTAssertTrue(model.updateConsoleAddress("192.168.1.35", expected: original, pairing: pairing))
        XCTAssertEqual(pairing.paired?.address, "192.168.1.35")
        XCTAssertEqual(pairing.paired?.registrationKey, original.registrationKey)
        XCTAssertEqual(engine.starts, 1)
    }
    @MainActor func testCaptureLocksSetupUntilStopAcknowledgement() throws {
        let engine = FakeEngine(); engine.available = true
        let model = AppModel(engine: engine)
        let account = try Account(decimalID: "42")
        let console = PairedConsole(accountID: account.accountID, address: "192.168.1.20", mac: Data(repeating: 1, count: 6), name: "PS5",
                                    target: 1000100, registrationKey: Data(repeating: 2, count: 16), sessionKey: Data(repeating: 3, count: 16), keyType: 0)
        model.requestStart(account: account, console: console)
        XCTAssertTrue(model.active)
        engine.onState?(.streaming)
        model.requestStart(account: account, console: console)
        XCTAssertEqual(model.state, .streaming)
        XCTAssertEqual(engine.starts, 1)
        engine.onState?(.retrying)
        XCTAssertTrue(model.active)
        model.stop()
        XCTAssertFalse(model.active)
    }
    @MainActor func testEngineStatesReachTheUI() {
        let engine = FakeEngine()
        let model = AppModel(engine: engine)
        engine.onState?(.waitingOBS)
        XCTAssertEqual(model.state, .waitingOBS)
        engine.onState?(.streaming)
        XCTAssertEqual(model.state, .streaming)
        engine.onState?(.pinRequired)
        XCTAssertEqual(model.state, .pinRequired)
        engine.onState?(.failed)
        XCTAssertEqual(model.state, .failed)
    }
    @MainActor func testOpeningAppDoesNotStartCapture() {
        let engine = FakeEngine()
        let model = AppModel(engine: engine)
        XCTAssertEqual(engine.starts, 0)
        XCTAssertEqual(model.state, .idle)
    }
    @MainActor func testUnavailableEngineDoesNotClaimStreaming() {
        let engine = FakeEngine()
        let model = AppModel(engine: engine)
        model.requestStart()
        XCTAssertEqual(engine.starts, 1)
        XCTAssertEqual(model.state, .unavailable)
    }
    @MainActor func testStopPublishesAcknowledgement() {
        let engine = FakeEngine()
        let model = AppModel(engine: engine)
        model.stop()
        XCTAssertEqual(engine.stops, 1)
        XCTAssertEqual(model.state, .stopped)
    }
    @MainActor func testShutdownPublishesStoppedBeforeReturning() async {
        let model = AppModel(engine: FakeEngine())
        await model.shutdown()
        XCTAssertEqual(model.state, .stopped)
    }
}
