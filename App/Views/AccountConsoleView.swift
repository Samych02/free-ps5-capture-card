import SwiftUI
import AppKit

struct AccountConsoleView: View {
    @ObservedObject var model: AccountModel
    @ObservedObject var pairing: PairingModel
    @ObservedObject var capture: AppModel
    @State private var redirect = ""
    @State private var request: Task<Void, Never>?
    @State private var confirmForget = false
    @State private var browserError: String?
    @State private var showPairing = false
    @State private var confirmForgetConsole = false
    @State private var addressToEdit: PairedConsole?
    @State private var expanded: Bool
    @FocusState private var redirectFocused: Bool

    init(model: AccountModel, pairing: PairingModel, capture: AppModel) {
        self.model = model; self.pairing = pairing; self.capture = capture
        _expanded = State(initialValue: model.account == nil || pairing.paired == nil || model.awaitingRedirect || model.message != nil || pairing.message != nil)
    }

    var body: some View {
        UtilitySection(title: "PS5 & account", symbol: "playstation.logo") {
            if let console = pairing.paired {
                VStack(alignment: .leading, spacing: 4) {
                    Label(console.name.isEmpty ? "Paired PS5" : console.name, systemImage: "checkmark.circle")
                        .font(.subheadline.weight(.semibold))
                    Text("\(console.address) · Pairing saved on this Mac")
                        .font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
                }.fixedSize(horizontal: false, vertical: true)
            }
            DisclosureGroup(isExpanded: $expanded) {
                accountAndConsole.padding(.top, 10)
            } label: {
                Text(model.account == nil || pairing.paired == nil ? "Set up your PS5" : "Manage account and pairing")
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            if capture.active || capture.isStopping {
                Text("Stop capture to change account or pairing.").font(.caption).foregroundStyle(.secondary)
            }
        }
        .sheet(isPresented: $showPairing) { PairingSheet(model: pairing) }
        .sheet(item: $addressToEdit) { console in
            ChangeAddressSheet(pairing: pairing, capture: capture, console: console)
        }
        .onChange(of: model.account) { _, account in
            pairing.setAccount(account); browserError = nil
            if account == nil { expanded = true }
        }
        .onChange(of: pairing.paired) { _, console in expanded = console == nil }
        .onChange(of: model.message) { _, message in if message != nil { expanded = true } }
        .onChange(of: pairing.message) { _, message in if message != nil { expanded = true } }
        .onChange(of: model.awaitingRedirect) { _, waiting in
            if waiting { expanded = true; redirectFocused = true } else { redirect = "" }
        }
        .onDisappear {
            redirect = ""; request?.cancel()
            if model.awaitingRedirect || model.busy { model.cancel() }
        }
        .confirmationDialog("Forget the paired PS5?", isPresented: $confirmForgetConsole) {
            Button("Forget PS5", role: .destructive) { pairing.forget() }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Removes the PS5 registration keys from this app’s local credentials file. Your PSN account stays saved.")
        }
        .confirmationDialog("Forget the saved PSN account?", isPresented: $confirmForget) {
            Button("Forget Account", role: .destructive) { model.forget() }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Removes the saved account and paired PS5 keys from this app’s local credentials file. Historical Keychain entries are not touched.")
        }
    }

    private var accountAndConsole: some View {
        VStack(alignment: .leading, spacing: 14) {
            Label(model.account == nil ? "PSN sign-in required" : "PSN account saved on this Mac",
                  systemImage: model.account == nil ? "person.crop.circle" : "person.crop.circle.badge.checkmark")
                .font(.subheadline.weight(.medium))
            if model.awaitingRedirect {
                Text("Finish signing in on Sony’s website, then copy the final redirect URL from your browser and paste it here. Treat that URL as private.")
                    .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                SecureField("Final Sony redirect URL", text: $redirect)
                    .textFieldStyle(.roundedBorder).focused($redirectFocused).privacySensitive()
                    .disabled(model.busy).onSubmit { finishSignIn() }
                    .accessibilityLabel("Final Sony redirect URL, contains a private authorization code")
                HStack {
                    Button(model.busy ? "Signing in…" : "Finish Sign-in") { finishSignIn() }
                        .disabled(redirect.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || model.busy)
                    Button("Cancel Sign-in") { request?.cancel(); redirect = ""; model.cancel() }
                    if model.busy { ProgressView().controlSize(.small).accessibilityLabel("Signing in to PSN") }
                }
            } else {
                ViewThatFits(in: .horizontal) {
                    HStack { accountActions }
                    VStack(alignment: .leading, spacing: 8) { accountActions }
                }.disabled(capture.active || capture.isStopping)
            }
            if let message = model.message ?? browserError { InlineNotice(text: message) }
            Divider()
            if pairing.paired == nil {
                Text("Pair once using the Link Device code on your PS5.").font(.callout).foregroundStyle(.secondary)
            }
            ViewThatFits(in: .horizontal) {
                HStack { consoleActions }
                VStack(alignment: .leading, spacing: 8) { consoleActions }
            }.disabled(capture.active || capture.isStopping)
            if !showPairing, let message = pairing.message { InlineNotice(text: message) }
            DisclosureGroup("About saved credentials") {
                Text("Your Sony password stays in your browser. Account identity and PS5 keys are saved in an unencrypted local file restricted to your macOS user. Other programs running as you can access it. OAuth tokens and passcodes are never saved.")
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    .padding(.top, 4).frame(maxWidth: .infinity, alignment: .leading)
            }.font(.caption).frame(maxWidth: .infinity, alignment: .leading)
        }.frame(maxWidth: .infinity, alignment: .leading)
    }
    @ViewBuilder private var accountActions: some View {
        Button(model.account == nil ? "Sign in to PSN…" : "Switch PSN Account…") {
            request?.cancel(); redirect = ""; browserError = nil
            let url = model.begin()
            if !NSWorkspace.shared.open(url) {
                model.cancel(); browserError = "Could not open your default browser. Check its macOS configuration."
            }
        }.disabled(model.busy || pairing.busy)
        if model.account != nil || model.message == AuthError.storage.localizedDescription {
            Button(model.account == nil ? "Reset Local Credentials…" : "Forget Account…", role: .destructive) {
                confirmForget = true
            }.disabled(model.busy || pairing.busy)
        }
    }
    @ViewBuilder private var consoleActions: some View {
        Button(pairing.paired == nil ? "Pair PS5…" : "Replace Pairing…") { showPairing = true }
            .disabled(model.account == nil || model.awaitingRedirect || model.busy || pairing.busy)
        if pairing.paired != nil {
            Button("Change IP…") { addressToEdit = pairing.paired }
                .disabled(model.awaitingRedirect || model.busy || pairing.busy || capture.active)
            Button("Forget PS5…", role: .destructive) { confirmForgetConsole = true }
                .disabled(model.awaitingRedirect || model.busy || pairing.busy)
        }
    }
    private func finishSignIn() {
        guard !model.busy, !redirect.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        let text = redirect; redirect = ""
        request = Task { await model.complete(text) }
    }
}
