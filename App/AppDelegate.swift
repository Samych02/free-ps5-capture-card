import AppKit

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var model: AppModel?
    weak var account: AccountModel?
    weak var pairing: PairingModel?
    var reopenWindow: (() -> Void)?
    private var terminating = false
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !flag { reopenWindow?(); sender.activate() }
        return true
    }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard let model else { return .terminateNow }
        if !terminating {
            terminating = true
            account?.cancel()
            Task { @MainActor in
                await pairing?.shutdown()
                await model.shutdown()
                sender.reply(toApplicationShouldTerminate: true)
            }
        }
        return .terminateLater
    }
}
