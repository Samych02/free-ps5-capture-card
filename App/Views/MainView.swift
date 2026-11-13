import SwiftUI

struct MainView: View {
    @ObservedObject var model: AppModel
    @ObservedObject var account: AccountModel
    var body: some View {
        ScrollView {
        VStack(alignment: .leading, spacing: 24) {
            HStack(alignment: .top) {
                Image(systemName: "cable.connector")
                    .font(.system(size: 30, weight: .light))
                    .foregroundStyle(.secondary)
                VStack(alignment: .leading, spacing: 4) {
                    Text("Free PS5 Capture Card").font(.title2.bold())
                    Text("A direct connection to OBS.").foregroundStyle(.secondary)
                }
            }
            AccountConsoleView(model: account)
            GroupBox {
                VStack(alignment: .leading, spacing: 14) {
                    LabeledContent("Engine", value: status)
                    Text("Development build: console pairing and live PS5 audio/video capture are not implemented yet.")
                        .font(.callout).foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }.padding(8)
            }
            HStack {
                Label("No local video or audio playback", systemImage: "speaker.slash")
                    .font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("Start Capture") { model.requestStart() }
                    .disabled(true)
                    .help("Available after pairing and the media pipeline are implemented.")
            }
            Text("Closing this window leaves the app running. Choose Quit to exit.")
                .font(.caption).foregroundStyle(.secondary)
        }
        .padding(28)
        .frame(maxWidth: .infinity, alignment: .leading)
        }
        .frame(minWidth: 500, idealWidth: 540, minHeight: 420)
    }
    private var status: String {
        switch model.state {
        case .idle: return "Idle — not connected"
        case .stopped: return "Stopped"
        case .unavailable: return "Capture unavailable in this build"
        }
    }
}
