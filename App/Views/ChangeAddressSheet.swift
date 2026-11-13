import SwiftUI

struct ChangeAddressSheet: View {
    @ObservedObject var pairing: PairingModel
    @ObservedObject var capture: AppModel
    let console: PairedConsole
    @Environment(\.dismiss) private var dismiss
    @State private var address: String
    @State private var attemptedSave = false
    @FocusState private var addressFocused: Bool

    init(pairing: PairingModel, capture: AppModel, console: PairedConsole) {
        self.pairing = pairing; self.capture = capture; self.console = console
        _address = State(initialValue: console.address)
    }
    private var validAddress: Bool { (try? PairingRequest.validAddress(address)) != nil }
    private var currentPairing: Bool { pairing.paired == console }
    private var canSave: Bool { validAddress && currentPairing && !pairing.busy && !capture.active }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Change PS5 IP address").font(.title2.bold())
            Text("Use the new address of the same PS5. Your existing pairing keys stay saved—no Link Device code or PSN sign-in is needed.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            TextField("PS5 IP address", text: $address)
                .textFieldStyle(.roundedBorder).autocorrectionDisabled()
                .focused($addressFocused).onSubmit { save() }
                .disabled(capture.active || pairing.busy || !currentPairing)
                .accessibilityIdentifier("changePS5Address")
            Text("Find it in PS5 Settings → Network → Connection Status → View Connection Status. Enter an IP address without a URL or port.")
                .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            if capture.active {
                Text("Stop capture before changing the IP address.").font(.callout).foregroundStyle(.red)
            } else if !currentPairing || pairing.busy {
                Text(PairingError.pairingChanged.localizedDescription).font(.callout).foregroundStyle(.red)
            } else if !validAddress {
                Text(PairingError.address.localizedDescription).font(.callout).foregroundStyle(.red)
            } else if attemptedSave, let message = pairing.message {
                Text(message).font(.callout).foregroundStyle(.red)
            }
            Text("Saving updates the address only. The next capture attempt checks whether the PS5 is reachable and accepts the saved keys.")
                .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            HStack {
                Button("Cancel") { dismiss() }.keyboardShortcut(.cancelAction)
                Spacer()
                Button("Save IP") { save() }.keyboardShortcut(.defaultAction).disabled(!canSave)
            }
        }
        .padding(24).frame(width: 440)
        .onAppear { addressFocused = true }
    }
    private func save() {
        guard canSave else { return }
        attemptedSave = true
        if capture.updateConsoleAddress(address, expected: console, pairing: pairing) { dismiss() }
    }
}
