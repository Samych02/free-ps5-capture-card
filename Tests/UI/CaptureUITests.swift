import XCTest

final class CaptureUITests: XCTestCase {
    func testUnconfiguredAppShowsSafeDefaultsAndCopyableOutput() {
        let app = XCUIApplication()
        app.launchArguments = ["--ui-testing"]
        app.launch()
        XCTAssertTrue(app.staticTexts["Free PS5 Capture Card"].firstMatch.waitForExistence(timeout: 10))
        XCTAssertTrue(app.buttons["Pair PS5…"].exists)
        XCTAssertFalse(app.buttons["Start Capture"].isEnabled)
        XCTAssertTrue(app.staticTexts["tcp://127.0.0.1:9295"].exists)
        XCTAssertTrue(app.popUpButtons["Video codec"].exists)
        XCTAssertTrue(app.sliders["Bitrate"].exists)
        XCTAssertTrue(app.staticTexts["bitrateReadout"].exists)
        app.terminate()
    }
}
