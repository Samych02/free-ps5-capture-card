import SwiftUI

struct PairingSheet: View {
    @ObservedObject var model: PairingModel
    @Environment(\.dismiss) private var dismiss
    @State private var address = ""
    @State private var selectedID = ""
    @State private var pin = ""
    @State private var request: Task<Void, Never>?
    @FocusState private var focused: Field?
    private enum Field { case address, code }
    private var validAddress: Bool { (try? PairingRequest.validAddress(address)) != nil }
    private var validPIN: Bool { PairingRequest.validPIN(pin) }
    private var canPair: Bool { validAddress && validPIN && !model.busy }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            Label("Pair your PS5", systemImage: "link").font(.title2.bold())
            Text("On your PS5, open Settings → System → Remote Play, enable Remote Play and choose Link Device. Keep the console on and on the same network as this Mac.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            UtilitySection(title: "Find your console", symbol: "network") {
                if !model.consoles.isEmpty {
                    Picker("Discovered PS5", selection: $selectedID) {
                        Text("Enter an IP address manually").tag("")
                        ForEach(model.consoles) { console in
                            Text("\(console.name.isEmpty ? "PS5" : console.name) — \(console.address)\(console.standby ? " (Rest mode)" : "")").tag(console.id)
                        }
                    }
                }
                HStack(spacing: 10) {
                    if model.isDiscovering { ProgressView().controlSize(.small).accessibilityLabel("Searching for PS5 consoles") }
                    Text(model.isDiscovering ? "Searching the local network…" : model.consoles.isEmpty ? "No consoles found. You can enter an IP address." : "Choose your PS5 or enter an IP address.")
                        .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 0)
                    Button("Search Again") { model.discover() }.disabled(model.isDiscovering)
                }
                if let message = model.discoveryMessage { InlineNotice(text: message) }
                TextField("PS5 IP address", text: $address).textFieldStyle(.roundedBorder)
                    .focused($focused, equals: .address).autocorrectionDisabled()
                    .accessibilityLabel("PS5 IP address")
                if !address.isEmpty && !validAddress {
                    Text("Enter an IP address without a URL or port.").font(.caption).foregroundStyle(.red)
                }
                Text("Find it in PS5 Settings → Network → Connection Status → View Connection Status. Wake a resting PS5 using its controller first.")
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }.disabled(model.busy)
            VStack(alignment: .leading, spacing: 6) {
                Text("Link Device code").font(.subheadline.weight(.medium))
                SecureField("8-digit Link Device code", text: $pin).textFieldStyle(.roundedBorder)
                    .focused($focused, equals: .code).privacySensitive().disabled(model.busy)
                    .accessibilityLabel("8-digit PS5 Link Device pairing code, not the 4-digit login passcode")
                    .onSubmit { pair() }
                Text(pin.isEmpty || validPIN ? "Eight digits from Link Device—not your four-digit login passcode." : "Use exactly eight digits (0–9), including any leading zeros.")
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            if let message = model.message { InlineNotice(text: message) }
            HStack {
                Button("Cancel") { request?.cancel(); pin = ""; model.cancel(); dismiss() }
                    .keyboardShortcut(.cancelAction)
                Spacer()
                if model.busy { ProgressView().controlSize(.small).accessibilityLabel("Pairing with PS5") }
                Button(model.busy ? "Pairing…" : "Pair PS5") { pair() }
                    .keyboardShortcut(.defaultAction).disabled(!canPair)
            }
        }
        .padding(24).frame(width: 490)
        .interactiveDismissDisabled(model.busy)
        .onChange(of: selectedID) { _, id in
            if let console = model.consoles.first(where: { $0.id == id }) {
                address = console.address; focused = .code
            }
        }
        .onChange(of: address) { _, value in
            if let console = model.consoles.first(where: { $0.id == selectedID }),
               (try? PairingRequest.validAddress(value)) != (try? PairingRequest.validAddress(console.address)) { selectedID = "" }
        }
        .onChange(of: model.consoles) { _, consoles in
            if !selectedID.isEmpty && !consoles.contains(where: { $0.id == selectedID }) { selectedID = "" }
        }
        .task {
            address = model.paired?.address ?? ""; focused = address.isEmpty ? .address : .code
            model.discover()
        }
        .onDisappear {
            request?.cancel(); pin = ""; model.cancel()
            Task { await model.stopDiscovery() }
        }
    }
    private func pair() {
        guard canPair else { return }
        let code = pin; pin = ""
        let normalized = try? PairingRequest.validAddress(address)
        let expectedID = model.consoles.first(where: { $0.id == selectedID && (try? PairingRequest.validAddress($0.address)) == normalized })?.id
        request = Task {
            await model.pair(address: address, pin: code, expectedID: expectedID)
            if !Task.isCancelled, model.message == nil, model.paired != nil { dismiss() }
        }
    }
}
