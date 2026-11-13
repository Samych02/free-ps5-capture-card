import XCTest
@testable import FreePS5CaptureCard

@MainActor
final class FakeEngine: EngineControlling {
    var onState: ((EngineState) -> Void)?
    var starts = 0
    var stops = 0
    func start() -> Bool { starts += 1; return false }
    func stop() { stops += 1; onState?(.stopped) }
    func shutdown() async { stop() }
}

final class AppModelTests: XCTestCase {
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
