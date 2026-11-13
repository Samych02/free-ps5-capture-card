import XCTest
@testable import FreePS5CaptureCard

final class EngineClientTests: XCTestCase {
    @MainActor func testShutdownClearsCallbackBeforeReturning() async {
        let engine = NativeEngineClient()
        engine.onState = { _ in }
        await engine.shutdown()
        XCTAssertNil(engine.onState)
        XCTAssertFalse(engine.start())
    }
    @MainActor func testRealFrameworkDoesNotPretendCaptureWorks() {
        let engine = NativeEngineClient()
        XCTAssertFalse(engine.start())
    }
    @MainActor func testRealWorkerDeliversStopOnMainActor() async {
        let engine = NativeEngineClient()
        let stopped = expectation(description: "stop acknowledgement")
        engine.onState = { state in
            XCTAssertTrue(Thread.isMainThread)
            if state == .stopped { stopped.fulfill() }
        }
        engine.stop()
        await fulfillment(of: [stopped], timeout: 3)
    }
}
