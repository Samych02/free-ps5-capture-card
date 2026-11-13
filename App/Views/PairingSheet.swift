import SwiftUI

// The existing native utility direction: quiet system controls, no preview or
// gameplay dashboard. Secrets stay in masked controls and are cleared on submit.
struct PairingSheet: View {
    @ObservedObject var model: PairingModel
    @Environment(\.dismiss) private var dismiss
    @State private var address = ""
    @State private var selectedID = ""
    @State private var pin = ""
    @State private var request: Task<Void, Never>?
    @State private var searching = true

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Pair your PS5").font(.title2.bold())
            Text("On your PS5, enable Settings → System → Remote Play, then choose Link Device. Keep the PS5 on and connected to the same network as this Mac.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            GroupBox("Console") {
                VStack(alignment: .leading, spacing: 10) {
                    if !model.consoles.isEmpty {
                        Picker("Discovered PS5", selection: $selectedID) {
                            Text("Enter an IP address manually").tag("")
                            ForEach(model.consoles) { console in
                                Text("\(console.name.isEmpty ? "PS5" : console.name) — \(console.address)\(console.standby ? " (Rest mode)" : "")").tag(console.id)
                            }
                        }
                    } else {
                        Text(searching ? "Looking for PS5 consoles… You can enter an IP address below." : "No consoles found. Enter the PS5’s IP address or search again.")
                            .font(.callout).foregroundStyle(.secondary)
                    }
                    TextField("PS5 IP address", text: $address).textFieldStyle(.roundedBorder)
                        .accessibilityLabel("PS5 IP address")
                    Text("Find the IP in PS5 Settings → Network → Connection Status → View Connection Status.")
                        .font(.caption).foregroundStyle(.secondary)
                    Button("Search Again") { searching = true; model.discover() }
                }.padding(6)
            }.disabled(model.busy)
            SecureField("8-digit Link Device code", text: $pin).textFieldStyle(.roundedBorder)
                .accessibilityLabel("8-digit PS5 Link Device pairing code, not the 4-digit login passcode")
                .disabled(model.busy)
            if let message = model.message {
                Text(message).foregroundStyle(.red).font(.callout).fixedSize(horizontal: false, vertical: true)
            }
            HStack {
                Button("Cancel") { request?.cancel(); pin = ""; model.cancel(); dismiss() }
                    .keyboardShortcut(.cancelAction)
                Spacer()
                if model.busy { ProgressView().controlSize(.small) }
                Button("Pair PS5") {
                    let code = pin; pin = ""
                    let expectedID = model.consoles.first(where: { $0.id == selectedID && $0.address == address })?.id
                    request = Task {
                        await model.pair(address: address, pin: code, expectedID: expectedID)
                        if !Task.isCancelled, model.message == nil, model.paired != nil { dismiss() }
                    }
                }.keyboardShortcut(.defaultAction)
                    .disabled(model.busy || address.isEmpty || pin.isEmpty)
            }
        }
        .padding(24).frame(width: 480)
        .interactiveDismissDisabled(model.busy)
        .onChange(of: selectedID) { _, id in
            if let console = model.consoles.first(where: { $0.id == id }) { address = console.address }
        }
        .task {
            model.discover()
            try? await Task.sleep(for: .seconds(5))
            searching = false
        }
        .onDisappear {
            request?.cancel(); pin = ""; model.cancel()
            Task { await model.stopDiscovery() }
        }
    }
}
