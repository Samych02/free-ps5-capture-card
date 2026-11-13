import XCTest
import CaptureEngine
@testable import FreePS5CaptureCard

final class OptionsTests: XCTestCase {
    func testExactNativeModesAndBitrateUnits() {
        var options = StreamOptions()
        XCTAssertEqual(options.codec, .h265)
        XCTAssertEqual(options.native.codec, CAPTURE_H265)
        XCTAssertEqual(StreamOptions.Codec.allCases, [.h264, .h265, .hdr])
        options.codec = .h264; XCTAssertEqual(options.native.codec, CAPTURE_H264)
        options.codec = .hdr; XCTAssertEqual(options.native.codec, CAPTURE_H265_HDR)
        for bitrate in stride(from: 2000, through: 100000, by: 1000) {
            options.bitrate = bitrate
            XCTAssertTrue(options.isValid)
            XCTAssertEqual(options.native.bitrate_kbps, UInt32(bitrate))
        }
        options.bitrate = 100001; XCTAssertFalse(options.isValid)
    }
    @MainActor func testCopiedURLUsesSelectedPortAndInvalidPreferencesAreIgnored() throws {
        let name = UUID().uuidString
        let defaults = try XCTUnwrap(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let model = AppModel(engine: FakeEngine(), defaults: defaults)
        model.options.port = 4567
        XCTAssertEqual(model.obsURL, "tcp://127.0.0.1:4567")
        XCTAssertEqual(AppModel(engine: FakeEngine(), defaults: defaults).options.port, 4567)
        defaults.set(Data("invalid json".utf8), forKey: "streamOptions.v1")
        XCTAssertEqual(StreamOptions.load(from: defaults), StreamOptions())
    }
}
