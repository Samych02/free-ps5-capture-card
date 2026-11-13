import SwiftUI

@main
struct FreePS5CaptureCardApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @StateObject private var model = AppModel()
    @StateObject private var account = AccountModel()
    @StateObject private var pairing = PairingModel(service: NativeConsoleService(), store: KeychainCredentialStore())
    var body: some Scene {
        WindowGroup {
            MainView(model: model, account: account, pairing: pairing)
                .onAppear { delegate.model = model; delegate.account = account; delegate.pairing = pairing }
                .task {
                    #if DEBUG
                    // XCTest hosts the real app but must not touch the user's
                    // real Keychain. Unit tests inject an isolated store instead.
                    if NSClassFromString("XCTestCase") != nil { return }
                    #endif
                    account.restore()
                    pairing.setAccount(account.account)
                }
        }
            .defaultSize(width: 540, height: 640)
    }
}
