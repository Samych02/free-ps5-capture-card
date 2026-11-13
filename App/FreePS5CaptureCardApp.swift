import SwiftUI

@main
struct FreePS5CaptureCardApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @StateObject private var model = AppModel()
    var body: some Scene {
        WindowGroup {
            MainView(model: model)
                .onAppear { delegate.model = model }
        }
            .defaultSize(width: 540, height: 420)
    }
}
