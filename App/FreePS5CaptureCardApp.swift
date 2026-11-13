import SwiftUI

@main
struct FreePS5CaptureCardApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @StateObject private var model = AppModel(defaults: Self.preferences)
    @StateObject private var account = AccountModel()
    @StateObject private var pairing = PairingModel(service: NativeConsoleService(), store: KeychainCredentialStore())
    private static var preferences: UserDefaults {
        #if DEBUG
        if ProcessInfo.processInfo.arguments.contains("--ui-testing") {
            let defaults = UserDefaults(suiteName: "org.freeps5capturecard.uitests")!
            defaults.removePersistentDomain(forName: "org.freeps5capturecard.uitests")
            return defaults
        }
        #endif
        return .standard
    }
    var body: some Scene {
        WindowGroup {
            MainView(model: model, account: account, pairing: pairing)
                .onAppear { delegate.model = model; delegate.account = account; delegate.pairing = pairing }
                .task {
                    #if DEBUG
                    // XCTest hosts the real app but must not touch the user's
                    // real Keychain. Unit tests inject an isolated store instead.
                    if NSClassFromString("XCTestCase") != nil || ProcessInfo.processInfo.arguments.contains("--ui-testing") { return }
                    #endif
                    account.restore()
                    pairing.setAccount(account.account)
                }
        }
            .defaultSize(width: 540, height: 640)
    }
}
