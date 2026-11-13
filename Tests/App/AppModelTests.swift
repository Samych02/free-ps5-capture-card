import XCTest
@testable import FreePS5CaptureCard

@MainActor
final class FakeEngine: EngineControlling {
    var onState: ((EngineState) -> Void)?
    var starts = 0
    var stops = 0
    var available = false
    func configure(options: StreamOptions, account: Account, console: PairedConsole) -> Bool { available }
    func start() -> Bool { starts += 1; return available }
    func stop() { stops += 1; onState?(.stopped) }
    func shutdown() async { stop() }
}

final class AppModelTests: XCTestCase {
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
