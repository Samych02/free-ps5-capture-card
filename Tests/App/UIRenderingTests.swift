import AppKit
import SwiftUI
import XCTest
@testable import FreePS5CaptureCard

/// Renders actual views off-screen with fake credentials and engine only.
/// This exercises layout, not mouse/keyboard automation or live media acceptance.
final class UIRenderingTests: XCTestCase {
    @MainActor func testReleaseLayoutsRenderInBothAppearances() async throws {
        _ = NSApplication.shared
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let output = root.appendingPathComponent("build/ui-polish", isDirectory: true)
        try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
        let icon = NSApp.applicationIconImage
        defer { NSApp.applicationIconImage = icon }
        NSApp.applicationIconImage = NSImage(contentsOf: root.appendingPathComponent("App/Assets.xcassets/AppIcon.appiconset/icon_128x128@2x.png"))

        for dark in [false, true] {
            let suffix = dark ? "dark" : "light"
            let suite = "render-tests-\(UUID().uuidString)"
            let defaults = try XCTUnwrap(UserDefaults(suiteName: suite))
            defer { defaults.removePersistentDomain(forName: suite) }
            let store = FileCredentialStore(access: FakeCredentialFile())
            let account = AccountModel(service: PSNAuthService(transport: StubAuthTransport()), store: store)
            let service = FakeConsoleService(), pairing = PairingModel(service: service, store: store)
            let engine = FakeEngine(), capture = AppModel(engine: engine, defaults: defaults)

            try render(MainView(model: capture, account: account, pairing: pairing),
                       name: "setup-\(suffix)", width: 620, height: 820, dark: dark, output: output)

            let identity = try Account(decimalID: "42")
            let console = PairedConsole(accountID: identity.accountID, address: "192.168.1.20", mac: Data([1,2,3,4,5,6]),
                                        name: "Living Room PS5", target: 1000100, registrationKey: Data(repeating: 2, count: 16),
                                        sessionKey: Data(repeating: 3, count: 16), keyType: 0)
            try store.saveAccount(identity); try store.savePairing(console)
            account.restore(); pairing.setAccount(identity)
            engine.available = true; capture.requestStart(account: identity, console: console)
            engine.snapshot = EngineStatus(phase: .streaming, lastOutputMessage: "Last OBS reset: output session changed.",
                                           lastSourceMessage: "A source connection ended; capture recovered.",
                                           width: 1920, height: 1080, fpsNumerator: 60000, fpsDenominator: 1001,
                                           videoPackets: 1234567, audioPackets: 2345678, reconnects: 0, queuedBytes: 31744,
                                           outputDisconnects: 2, videoClockDriftMicroseconds: -91000,
                                           audioClockDriftMicroseconds: 6000, videoClockElapsedMicroseconds: 49000000,
                                           audioClockElapsedMicroseconds: 49000000, videoClockPosition: 2930,
                                           audioClockPosition: 4900, audioSamplesPerPacket: 480)
            engine.onState?(.streaming); capture.showOutputMetrics = true
            try render(MainView(model: capture, account: account, pairing: pairing),
                       name: "streaming-\(suffix)", width: dark ? 620 : 540, height: 1000, dark: dark, output: output)
            try render(OutputMetricsView(status: capture.status).padding(20),
                       name: "metrics-\(suffix)", width: 480, height: 520, dark: dark, output: output)
            try render(CaptureStatusView(presentation: CapturePresentation(state: .streaming, status: capture.status, isStopping: true)).padding(20),
                       name: "stopping-\(suffix)", width: 500, height: 160, dark: dark, output: output)
            var failure = EngineStatus()
            failure.message = "The PS5 rejected this connection. Check that Remote Play is enabled and this account is allowed to connect. Stop capture before trying again."
            try render(CaptureStatusView(presentation: CapturePresentation(state: .failed, status: failure)).padding(20),
                       name: "failure-\(suffix)", width: 500, height: 200, dark: dark, output: output)

            for (name, phase) in [("protected", EnginePhase.protectedContent), ("paused", .paused), ("obs-wait", .waitingOBS), ("media-wait", .waitingMedia)] {
                engine.snapshot.phase = phase; engine.onState?(.waitingOBS)
                try render(CaptureStatusView(presentation: CapturePresentation(state: capture.state, status: capture.status)).padding(20),
                           name: "\(name)-\(suffix)", width: 500, height: 160, dark: dark, output: output)
            }
            capture.stop()
            try render(StreamSettingsView(model: capture).padding(20),
                       name: "settings-\(suffix)", width: 540, height: 450, dark: dark, output: output)
            capture.portText = "not-a-port"; capture.options.codec = .hdr
            try render(StreamSettingsView(model: capture).padding(20),
                       name: "invalid-settings-\(suffix)", width: 540, height: 600, dark: dark, output: output)
            capture.portText = "9295"
            try render(PairingSheet(model: pairing), name: "pairing-\(suffix)", width: 490, height: 560, dark: dark, output: output)
            try render(ChangeAddressSheet(pairing: pairing, capture: capture, console: console),
                       name: "change-ip-\(suffix)", width: 440, height: 420, dark: dark, output: output)
            engine.snapshot = EngineStatus(); engine.onState?(.pinRequired)
            try render(LoginPasscodeSheet(model: capture), name: "passcode-\(suffix)", width: 400, height: 310, dark: dark, output: output)
            _ = account.begin()
            try render(AccountConsoleView(model: account, pairing: pairing, capture: capture),
                       name: "sign-in-\(suffix)", width: 540, height: 550, dark: dark, output: output)
            account.cancel(); await pairing.stopDiscovery()
        }
    }

    @MainActor private func render<V: View>(_ view: V, name: String, width: CGFloat, height: CGFloat, dark: Bool, output: URL) throws {
        let host = NSHostingView(rootView: view.preferredColorScheme(dark ? .dark : .light)
            .frame(width: width, height: height).background(Color(nsColor: .windowBackgroundColor)))
        host.appearance = NSAppearance(named: dark ? .darkAqua : .aqua)
        let window = NSWindow(contentRect: NSRect(x: -12000, y: -12000, width: width, height: height),
                              styleMask: [.borderless], backing: .buffered, defer: false)
        window.isReleasedWhenClosed = false; window.contentView = host
        defer { window.close() }
        host.layoutSubtreeIfNeeded(); host.displayIfNeeded()
        let bitmap = try XCTUnwrap(host.bitmapImageRepForCachingDisplay(in: host.bounds))
        host.cacheDisplay(in: host.bounds, to: bitmap)
        let png = try XCTUnwrap(bitmap.representation(using: .png, properties: [:]))
        XCTAssertGreaterThan(png.count, 2000, "Layout rendered an unexpectedly empty image: \(name)")
        try png.write(to: output.appendingPathComponent(name + ".png"), options: .atomic)
    }
}
