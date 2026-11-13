import XCTest
@testable import FreePS5CaptureCard

final class EngineClientTests: XCTestCase {
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
