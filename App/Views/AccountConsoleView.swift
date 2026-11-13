import SwiftUI
import AppKit

struct AccountConsoleView: View {
    @ObservedObject var model: AccountModel
    @State private var redirect = ""
    @State private var request: Task<Void, Never>?
    @State private var confirmForget = false
    @State private var browserError: String?

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
                Text("Your Sony password stays in your browser. Only your pairing identity is saved in Keychain; OAuth tokens are not retained.")
                    .font(.caption).foregroundStyle(.secondary)
                Divider()
                LabeledContent("PS5", value: "Pairing is the next milestone")
            }.padding(8)
        }
        .confirmationDialog("Forget the saved PSN account?", isPresented: $confirmForget) {
            Button("Forget Account", role: .destructive) { model.forget() }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("The saved pairing identity will be removed from this app’s Keychain record.")
        }
    }
}
