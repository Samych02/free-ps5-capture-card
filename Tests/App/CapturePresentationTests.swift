import XCTest
import AppKit
@testable import FreePS5CaptureCard

final class CapturePresentationTests: XCTestCase {
    func testHealthyStreamDoesNotPresentHistoryAsCurrentError() {
        var status = EngineStatus()
        status.phase = .streaming
        status.lastOutputMessage = "Last OBS reset: output session changed."
        status.lastSourceMessage = "Source timing discontinuity."
        let result = CapturePresentation(state: .streaming, status: status)
        XCTAssertEqual(result.title, "Streaming to OBS")
        XCTAssertEqual(result.tone, .success)
        XCTAssertFalse(result.detail.contains("reset"))
        XCTAssertFalse(result.detail.contains("discontinuity"))
    }
    func testProtectedContentOverridesStaleStreamingFlag() {
        var status = EngineStatus(); status.phase = .protectedContent
        let result = CapturePresentation(state: .streaming, status: status)
        XCTAssertEqual(result.title, "Protected content")
        XCTAssertEqual(result.tone, .warning)
        XCTAssertTrue(result.detail.contains("game"))
        XCTAssertFalse(result.isProgress)
    }
    func testMediaPauseAndOBSWaitHaveDifferentExplanations() {
        var status = EngineStatus(); status.phase = .paused
        let paused = CapturePresentation(state: .waitingOBS, status: status)
        status.phase = .waitingOBS
        let waiting = CapturePresentation(state: .waitingOBS, status: status)
        XCTAssertEqual(paused.title, "Waiting for PS5 media")
        XCTAssertEqual(waiting.title, "Waiting for OBS")
        XCTAssertNotEqual(paused.detail, waiting.detail)
    }
    func testEveryStateHasUsefulAccessibleText() {
        for state in [EngineState.idle, .stopped, .connecting, .waitingOBS, .streaming, .retrying, .pinRequired, .failed, .unavailable] {
            let result = CapturePresentation(state: state, status: EngineStatus())
            XCTAssertFalse(result.title.isEmpty)
            XCTAssertFalse(result.detail.isEmpty)
            XCTAssertFalse(result.symbol.isEmpty)
            XCTAssertNotNil(NSImage(systemSymbolName: result.symbol, accessibilityDescription: nil))
        }
    }
    func testTerminalErrorAndPINAreNotHiddenByMediaPhase() {
        var status = EngineStatus(); status.phase = .protectedContent; status.message = "A current error."
        XCTAssertEqual(CapturePresentation(state: .failed, status: status).detail, status.message)
        XCTAssertEqual(CapturePresentation(state: .pinRequired, status: status).title, "PS5 passcode required")
    }
    func testStoppingIsExplicitAndDoesNotLookLikeStreaming() {
        var status = EngineStatus(); status.phase = .streaming
        let result = CapturePresentation(state: .streaming, status: status, isStopping: true)
        XCTAssertEqual(result.title, "Stopping capture…")
        XCTAssertTrue(result.isProgress)
        XCTAssertNotEqual(result.tone, .success)
    }
}
