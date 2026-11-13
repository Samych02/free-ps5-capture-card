import SwiftUI

@main
struct FreePS5CaptureCardApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @StateObject private var model = AppModel()
    @StateObject private var account = AccountModel()
    var body: some Scene {
        WindowGroup {
            MainView(model: model, account: account)
                .onAppear { delegate.model = model; delegate.account = account }
                .task {
                    #if DEBUG
                    // XCTest hosts the real app but must not touch the user's
                    // real Keychain. Unit tests inject an isolated store instead.
                    if NSClassFromString("XCTestCase") != nil { return }
                    #endif
                    account.restore()
                }
        }
            .defaultSize(width: 540, height: 640)
    }
}
