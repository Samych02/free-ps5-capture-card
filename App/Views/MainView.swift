import SwiftUI
import AppKit

struct MainView: View {
    @ObservedObject var model: AppModel
    @ObservedObject var account: AccountModel
    @ObservedObject var pairing: PairingModel
    @State private var copied = false
    @State private var copyFeedback: Task<Void, Never>?

    private var presentation: CapturePresentation {
        CapturePresentation(state: model.state, status: model.status, isStopping: model.isStopping)
    }
    private var startBlocker: String? {
        if account.awaitingRedirect || account.busy { return "Finish or cancel PSN sign-in before starting capture." }
        if account.account == nil { return "Sign in to PSN and pair your PS5 to get started." }
        if pairing.busy { return "Wait for pairing to finish." }
        if pairing.paired == nil { return "Pair your PS5 before starting capture." }
        if !model.optionsAreValid { return "Enter a valid local TCP port in Stream settings." }
        return nil
    }
    var body: some View {
        VStack(spacing: 0) {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 12) {
                    Image(nsImage: NSApp.applicationIconImage).resizable().frame(width: 48, height: 48).accessibilityHidden(true)
                    VStack(alignment: .leading, spacing: 3) {
                        Text("Free PS5 Capture Card").font(.title2.bold())
                        Text("Your PS5. One OBS source. Video and audio.").font(.callout).foregroundStyle(.secondary)
                    }
                    Spacer(minLength: 0)
                }
                CaptureStatusView(presentation: presentation)
                HStack(spacing: 10) {
                    Button("Start Capture", systemImage: "play.fill") {
                        model.requestStart(account: account.account, console: pairing.paired)
                    }
                    .buttonStyle(.borderedProminent).controlSize(.large)
                    .disabled(model.active || model.isStopping || startBlocker != nil)
                    .help(startBlocker ?? "Start forwarding your PS5’s compressed video and audio to OBS.")
                    Button(model.isStopping ? "Stopping…" : "Stop", systemImage: "stop.fill") { model.stop() }
                        .controlSize(.large).disabled(!model.active || model.isStopping)
                    Spacer()
                }
                if !model.active, let blocker = startBlocker {
                    Text(blocker).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                } else if model.state == .failed {
                    Text("Stop to reset capture before trying again or changing setup.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
            .padding(24).frame(maxWidth: 760, alignment: .leading)
            Divider()
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    AccountConsoleView(model: account, pairing: pairing, capture: model)
                        .help(model.active ? "Stop capture before changing account or pairing." : "Configure your PSN identity and PS5.")
                    outputSection
                    StreamSettingsView(model: model)
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No preview or local playback. Closing this window keeps capture running; Quit disconnects from the PS5.")
                        Text("Version \(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.1.0") · Release candidate")
                    }
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                }
                .padding(24).frame(maxWidth: 760, alignment: .leading).frame(maxWidth: .infinity)
            }
        }
        .tint(.blue)
        .frame(minWidth: 540, idealWidth: 620, maxWidth: 900, minHeight: 600)
        .sheet(isPresented: Binding(get: { model.state == .pinRequired && !model.isStopping }, set: { presented in
            // A native state transition can dismiss the sheet after successful
            // submission. Only an actual dismissal while still requesting PIN
            // should stop the session.
            if !presented { model.dismissPINSheet() }
        })) {
            LoginPasscodeSheet(model: model)
        }
        .onChange(of: model.obsURL) { _, _ in copyFeedback?.cancel(); copied = false }
        .onDisappear { copyFeedback?.cancel(); copied = false }
    }
    private var outputSection: some View {
        UtilitySection(title: "OBS connection", symbol: "link") {
            HStack(alignment: .center, spacing: 12) {
                Text(model.obsURL).font(.system(.body, design: .monospaced))
                    .textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
                Button(copied ? "Copied" : "Copy URL", systemImage: copied ? "checkmark" : "doc.on.doc") { copyURL() }
                    .disabled(!model.optionsAreValid).accessibilityIdentifier("copyOBSURL")
            }
            Text("In OBS, add a Media Source, turn off Local File, paste this URL and set Input Format to matroska. This one source carries both video and audio.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            DisclosureGroup(isExpanded: $model.showOutputMetrics) {
                OutputMetricsView(status: model.status)
            } label: {
                Text(model.showOutputMetrics ? "Hide metrics" : "Show metrics").font(.callout)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityIdentifier("outputMetricsDisclosure")
        }
    }
    private func copyURL() {
        guard model.optionsAreValid else { return }
        NSPasteboard.general.clearContents()
        guard NSPasteboard.general.setString(model.obsURL, forType: .string) else { return }
        copyFeedback?.cancel(); copied = true
        copyFeedback = Task {
            do { try await Task.sleep(for: .seconds(2)) } catch { return }
            copied = false
        }
    }
}
