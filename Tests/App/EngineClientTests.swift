import XCTest
import CaptureEngine
@testable import FreePS5CaptureCard

final class EngineClientTests: XCTestCase {
    func testNativeStatusPreservesSignedClockDriftAndCounterSpans() {
        var value = capture_status()
        value.video_clock_drift_us = -2_823_000; value.audio_clock_drift_us = 700
        value.video_clock_elapsed_us = 180_000_000; value.audio_clock_elapsed_us = 179_990_000
        value.video_clock_position = 10620; value.audio_clock_position = 17999
        value.audio_samples_per_packet = 480
        value.pin_request_id = UInt64.max - 1
        let result = NativeEngineClient.decodedStatus(value)
        XCTAssertEqual(result.videoClockDriftMicroseconds, -2_823_000)
        XCTAssertEqual(result.audioClockDriftMicroseconds, 700)
        XCTAssertEqual(result.videoClockElapsedMicroseconds, 180_000_000)
        XCTAssertEqual(result.audioClockElapsedMicroseconds, 179_990_000)
        XCTAssertEqual(result.videoClockPosition, 10620)
        XCTAssertEqual(result.audioClockPosition, 17999)
        XCTAssertEqual(result.audioSamplesPerPacket, 480)
        XCTAssertEqual(result.pinRequestID, UInt64.max - 1)
    }
    func testNativeStatusKeepsOutputResetCauseSeparateFromSourceRetries() {
        var value = capture_status()
        value.width = 1920; value.video_packets = 500; value.audio_packets = 600
        value.reconnects = 2; value.output_disconnects = 3
        let message = "Last OBS reset: queued media became stale."
        withUnsafeMutableBytes(of: &value.last_output_message) { $0.copyBytes(from: message.utf8) }
        let source = "A source connection ended."
        withUnsafeMutableBytes(of: &value.last_source_message) { $0.copyBytes(from: source.utf8) }
        value.state = CAPTURE_WAITING_OBS; value.phase = CAPTURE_PHASE_CONTENT_BLOCKED
        let result = NativeEngineClient.decodedStatus(value)
        XCTAssertEqual(result.outputDisconnects, 3)
        XCTAssertEqual(result.reconnects, 2)
        XCTAssertEqual(result.videoPackets, 500)
        XCTAssertEqual(result.audioPackets, 600)
        XCTAssertEqual(result.width, 1920)
        XCTAssertEqual(result.message, "")
        XCTAssertEqual(result.lastOutputMessage, message)
        XCTAssertEqual(result.lastSourceMessage, source)
        XCTAssertEqual(result.nativeState, .waitingOBS)
        XCTAssertEqual(result.phase, .protectedContent)
    }
    @MainActor func testConfigurationRequiresMatchingSavedIdentityWithoutStarting() throws {
        let engine = NativeEngineClient()
        let account = try Account(decimalID: "42")
        let console = PairedConsole(accountID: account.accountID, address: "192.168.1.20", mac: Data(repeating: 1, count: 6),
                                    name: "PS5", target: 1000100, registrationKey: Data(repeating: 2, count: 16),
                                    sessionKey: Data(repeating: 3, count: 16), keyType: 0)
        XCTAssertTrue(engine.configure(options: StreamOptions(), account: account, console: console))
        XCTAssertFalse(engine.configure(options: StreamOptions(), account: try Account(decimalID: "43"), console: console))
        XCTAssertEqual(engine.status().videoPackets, 0)
        XCTAssertFalse(engine.submitPIN("1234"))
    }
    func testStreamOptionsPersistWithoutSecrets() throws {
        let name = UUID().uuidString
        let defaults = try XCTUnwrap(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        var options = StreamOptions()
        XCTAssertTrue(options.isValid)
        options.codec = .hdr; options.height = 720; options.fps = 30; options.bitrate = 22000; options.port = 12345
        options.save(to: defaults)
        XCTAssertEqual(StreamOptions.load(from: defaults), options)
        XCTAssertEqual(options.native.width, 1280)
        XCTAssertEqual(options.native.port, 12345)
        options.port = 65536
        XCTAssertFalse(options.isValid)
        options.save(to: defaults)
        XCTAssertEqual(StreamOptions.load(from: defaults).port, 12345)
    }
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
