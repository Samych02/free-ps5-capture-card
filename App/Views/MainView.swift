import SwiftUI
import AppKit

struct MainView: View {
    @ObservedObject var model: AppModel
    @ObservedObject var account: AccountModel
    @ObservedObject var pairing: PairingModel
    @State private var loginPIN = ""
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                HStack(alignment: .top, spacing: 12) {
                    Image(systemName: "cable.connector").font(.largeTitle).foregroundStyle(.secondary)
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Free PS5 Capture Card").font(.title2.bold())
                        Text("Compressed audio and video. Straight to OBS.").foregroundStyle(.secondary)
                    }
                }
                AccountConsoleView(model: account, pairing: pairing, capture: model)
                    .disabled(model.active)
                    .help(model.active ? "Stop capture before changing or forgetting credentials." : "Configure your PSN identity and PS5.")
                GroupBox("Stream options") {
                    Form {
                        Picker("Video codec", selection: $model.options.codec) {
                            Text("H264").tag(StreamOptions.Codec.h264)
                            Text("H265 (Default)").tag(StreamOptions.Codec.h265)
                            Text("H265 HDR").tag(StreamOptions.Codec.hdr)
                        }
                        Picker("Resolution", selection: $model.options.height) {
                            Text("1080p").tag(1080); Text("720p").tag(720)
                        }
                        Picker("Frame rate", selection: $model.options.fps) {
                            Text("60 fps").tag(60); Text("30 fps").tag(30)
                        }
                        VStack(alignment: .leading, spacing: 4) {
                            Text("Bitrate: \(model.options.bitrate / 1000) Mbps")
                                .monospacedDigit().accessibilityIdentifier("bitrateReadout")
                            Slider(value: Binding(get: { Double(model.options.bitrate) }, set: { model.options.bitrate = Int($0) }),
                                   in: 2000...100000, step: 1000) { Text("Bitrate") }
                            Text("Recommended starting point: 15 Mbps").font(.caption).foregroundStyle(.secondary)
                        }
                        TextField("Local TCP port", value: $model.options.port, format: .number.grouping(.never))
                            .textFieldStyle(.roundedBorder)
                            .help("1024–65535. OBS must use the same port. Defaults to 9295.")
                    }.padding(8).disabled(model.active)
                    if !model.options.isValid { Text("Choose a port from 1024 to 65535.").foregroundStyle(.red) }
                    if model.options.codec == .hdr {
                        Text("HDR passthrough requires an HDR-capable PS5 game and correct OBS colour settings. No tone mapping or silent SDR fallback.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                GroupBox("OBS output") {
                    VStack(alignment: .leading, spacing: 12) {
                        LabeledContent("Status", value: status)
                        if !model.status.message.isEmpty {
                            Text(model.status.message).font(.callout).textSelection(.enabled)
                        }
                        HStack {
                            Text(model.obsURL).font(.system(.body, design: .monospaced)).textSelection(.enabled)
                            Spacer()
                            Button("Copy URL", systemImage: "doc.on.doc") {
                                NSPasteboard.general.clearContents()
                                NSPasteboard.general.setString(model.obsURL, forType: .string)
                            }.disabled(!model.options.isValid)
                        }
                        Text("In OBS, add one Media Source, turn off Local File, paste this URL, and choose matroska as the input format. Both tracks use this source.")
                            .font(.caption).foregroundStyle(.secondary)
                        if model.status.width > 0 {
                            Text("Received: \(model.status.width)×\(model.status.height) · nominal \(cadence) fps · video \(model.status.videoPackets) · audio \(model.status.audioPackets)")
                                .font(.caption).monospacedDigit().accessibilityIdentifier("receivedMediaStatus")
                            Text("Queue: \(model.status.queuedBytes / 1024) KiB / \(model.status.queueAgeMicroseconds / 1000) ms · video lost \(model.status.lostVideoFrames) · audio missing \(model.status.missingAudioPackets) · source retries \(model.status.reconnects) · OBS resets \(model.status.outputDisconnects)")
                                .font(.caption).foregroundStyle(.secondary).monospacedDigit()
                            if model.status.videoClockElapsedMicroseconds > 0 {
                                Text("Nominal counter drift vs ingress: video \(model.status.videoClockDriftMicroseconds / 1000) ms · audio \(model.status.audioClockDriftMicroseconds / 1000) ms")
                                    .font(.caption).foregroundStyle(.secondary).monospacedDigit().accessibilityIdentifier("sourceClockDrift")
                                Text("Observed counters: video \(counterRate(model.status.videoClockPosition, model.status.videoClockElapsedMicroseconds))/s · audio \(counterRate(model.status.audioClockPosition, model.status.audioClockElapsedMicroseconds))/s · \(model.status.audioSamplesPerPacket) samples/audio packet")
                                    .font(.caption).foregroundStyle(.secondary).monospacedDigit().accessibilityIdentifier("sourceClockRates")
                            }
                        }
                        HStack {
                            Button("Start Capture", systemImage: "play.fill") {
                                model.requestStart(account: account.account, console: pairing.paired)
                            }
                            .buttonStyle(.borderedProminent)
                            .disabled(model.active || !model.options.isValid || account.account == nil || pairing.paired == nil || account.busy || account.awaitingRedirect || pairing.busy)
                            Button("Stop", systemImage: "stop.fill") { model.stop() }.disabled(!model.active)
                        }
                    }.padding(8)
                }
                Text("No preview or local playback. Stop before changing setup. Closing the window leaves capture running; Quit disconnects from the PS5.")
                    .font(.caption).foregroundStyle(.secondary)
            }.padding(26).frame(maxWidth: .infinity, alignment: .leading)
        }
        .frame(minWidth: 500, idealWidth: 560, minHeight: 500)
        .sheet(isPresented: Binding(get: { model.state == .pinRequired }, set: { if !$0 { model.stop() } })) {
            VStack(alignment: .leading, spacing: 16) {
                Text("PS5 login passcode").font(.headline)
                Text("Enter the 4-digit console login passcode. This is not the 8-digit Link Device code. It is used once and never saved.")
                if !model.status.message.isEmpty { Text(model.status.message).font(.callout).foregroundStyle(.red) }
                SecureField("4-digit login passcode", text: $loginPIN).textFieldStyle(.roundedBorder)
                HStack {
                    Button("Cancel") { loginPIN = ""; model.stop() }
                    Button("Continue") {
                        _ = model.submitPIN(loginPIN); loginPIN = ""
                    }.disabled(loginPIN.utf8.count != 4 || !loginPIN.utf8.allSatisfy({ (48...57).contains($0) }))
                        .keyboardShortcut(.defaultAction)
                }
            }.padding(24).frame(width: 380)
                .onDisappear { loginPIN = "" }
        }
    }
    private var cadence: String {
        guard model.status.fpsDenominator > 0 else { return "unknown" }
        return String(format: "%.2f", Double(model.status.fpsNumerator) / Double(model.status.fpsDenominator))
    }
    private func counterRate(_ position: UInt64, _ elapsedMicroseconds: UInt64) -> String {
        guard elapsedMicroseconds > 0 else { return "unknown" }
        return String(format: "%.3f", Double(position) * 1_000_000 / Double(elapsedMicroseconds))
    }
    private var status: String {
        switch model.state {
        case .idle: return "Idle — not connected"
        case .stopped: return "Stopped"
        case .connecting: return "Connecting to PS5…"
        case .waitingOBS: return "Connected — waiting for OBS and both tracks"
        case .streaming: return "Sending audio and video to OBS"
        case .retrying: return "Reconnecting…"
        case .pinRequired: return "PS5 login passcode required"
        case .failed: return "Capture failed — stop before retrying"
        case .unavailable: return "Cannot start — check pairing and stream options"
        }
    }
}
