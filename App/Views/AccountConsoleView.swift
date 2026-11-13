import SwiftUI
import AppKit

struct AccountConsoleView: View {
    @ObservedObject var model: AccountModel
    @ObservedObject var pairing: PairingModel
    @State private var redirect = ""
    @State private var request: Task<Void, Never>?
    @State private var confirmForget = false
    @State private var browserError: String?
    @State private var showPairing = false
    @State private var confirmForgetConsole = false

    var body: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 12) {
                LabeledContent("PSN Account", value: model.account == nil ? "Not configured" : "Saved securely")
                if model.awaitingRedirect {
                    Text("Finish signing in on Sony’s website. Copy the final redirect URL from the browser and paste it below.")
                        .font(.callout).foregroundStyle(.secondary)
                    SecureField("Final Sony redirect URL", text: $redirect)
                        .textFieldStyle(.roundedBorder)
                        .disabled(model.busy)
                        .accessibilityLabel("Final Sony redirect URL, contains a private authorization code")
                    HStack {
                        Button("Finish Sign-in") {
                            let text = redirect
                            redirect = ""
                            request = Task { await model.complete(text) }
                        }.disabled(redirect.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || model.busy)
                        Button("Cancel") { request?.cancel(); redirect = ""; model.cancel() }
                        if model.busy { ProgressView().controlSize(.small) }
                    }
                } else {
                    HStack {
                        Button(model.account == nil ? "Sign in to PSN…" : "Sign in with another account…") {
                            request?.cancel(); redirect = ""; browserError = nil
                            let url = model.begin()
                            if !NSWorkspace.shared.open(url) {
                                model.cancel()
                                browserError = "Could not open your default browser. Check its macOS configuration."
                            }
                        }
                        if model.account != nil {
                            Button("Forget Account…", role: .destructive) { confirmForget = true }
                        }
                    }
                }
                if let message = model.message ?? browserError {
                    Text(message).font(.callout).foregroundStyle(.red).textSelection(.enabled)
                }
                Text("Your Sony password stays in your browser. Account identity and paired PS5 keys stay in Keychain; OAuth tokens are not retained.")
                    .font(.caption).foregroundStyle(.secondary)
                Divider()
                LabeledContent("PS5", value: consoleName)
                if let console = pairing.paired {
                    Text("\(console.address) · Saved securely").font(.caption).foregroundStyle(.secondary)
                }
                HStack {
                    Button(pairing.paired == nil ? "Pair PS5…" : "Replace Pairing…") { showPairing = true }
                        .disabled(model.account == nil || model.awaitingRedirect || model.busy)
                    if pairing.paired != nil {
                        Button("Forget PS5…", role: .destructive) { confirmForgetConsole = true }
                    }
                }
                if !showPairing, let message = pairing.message {
                    Text(message).font(.callout).foregroundStyle(.red)
                }
            }.padding(8)
        }
        .sheet(isPresented: $showPairing) { PairingSheet(model: pairing) }
        .onChange(of: model.account) { _, account in pairing.setAccount(account) }
        .confirmationDialog("Forget the paired PS5?", isPresented: $confirmForgetConsole) {
            Button("Forget PS5", role: .destructive) { pairing.forget() }
            Button("Cancel", role: .cancel) {}
        } message: { Text("The PS5 registration keys will be removed from Keychain. Your PSN identity will remain.") }
        .confirmationDialog("Forget the saved PSN account?", isPresented: $confirmForget) {
            Button("Forget Account", role: .destructive) { model.forget() }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("The account identity and any paired PS5 keys will be removed from this app’s Keychain record.")
        }
    }
    private var consoleName: String {
        guard let console = pairing.paired else { return "Not paired" }
        return console.name.isEmpty ? "Paired PS5" : console.name
    }
}
