import SwiftUI

struct LoginPasscodeSheet: View {
    @ObservedObject var model: AppModel
    @State private var passcode = ""
    @FocusState private var focused: Bool
    private var valid: Bool { passcode.utf8.count == 4 && passcode.utf8.allSatisfy({ (48...57).contains($0) }) }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Label("PS5 login passcode", systemImage: "lock").font(.title2.bold())
            Text("Enter the four-digit passcode used to sign in to your console—not the eight-digit Link Device code. It is used once and never saved.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            SecureField("4-digit login passcode", text: $passcode)
                .textFieldStyle(.roundedBorder).focused($focused).privacySensitive()
                .disabled(model.isSubmittingPIN || model.isStopping)
                .onSubmit { submit() }
                .accessibilityIdentifier("consoleLoginPasscode")
            if let message = model.pinSubmissionError {
                InlineNotice(text: message)
            } else if !model.status.message.isEmpty {
                InlineNotice(text: model.status.message)
            } else if !passcode.isEmpty && !valid {
                Text("Use exactly four digits (0–9).").font(.caption).foregroundStyle(.secondary)
            }
            HStack {
                Button("Cancel") { passcode = ""; model.stop() }.keyboardShortcut(.cancelAction)
                    .disabled(model.isStopping)
                Spacer()
                if model.isSubmittingPIN { ProgressView().controlSize(.small).accessibilityLabel("Submitting passcode") }
                Button(model.isSubmittingPIN ? "Submitting…" : "Continue") { submit() }
                    .keyboardShortcut(.defaultAction)
                    .disabled(!valid || model.isSubmittingPIN || model.isStopping)
            }
        }
        .padding(24).frame(width: 400)
        .interactiveDismissDisabled(model.isSubmittingPIN || model.isStopping)
        .onAppear { focused = true }
        .onDisappear { passcode = "" }
    }
    private func submit() {
        guard valid, !model.isSubmittingPIN, !model.isStopping else { return }
        let value = passcode; passcode = ""
        _ = model.submitPIN(value)
    }
}
