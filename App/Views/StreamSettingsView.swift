import SwiftUI

struct StreamSettingsView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        UtilitySection(title: "Stream settings", symbol: "slider.horizontal.3") {
            Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 12) {
                GridRow {
                    Text("Video codec").foregroundStyle(.secondary)
                    Picker("Video codec", selection: $model.options.codec) {
                        Text("H264").tag(StreamOptions.Codec.h264)
                        Text("H265 (Default)").tag(StreamOptions.Codec.h265)
                        Text("H265 HDR").tag(StreamOptions.Codec.hdr)
                    }.labelsHidden().frame(maxWidth: .infinity, alignment: .leading)
                }
                GridRow {
                    Text("Resolution").foregroundStyle(.secondary)
                    Picker("Resolution", selection: $model.options.height) {
                        Text("1080p").tag(1080); Text("720p").tag(720)
                    }.labelsHidden()
                }
                GridRow {
                    Text("Frame rate").foregroundStyle(.secondary)
                    Picker("Frame rate", selection: $model.options.fps) {
                        Text("60 fps").tag(60); Text("30 fps").tag(30)
                    }.labelsHidden()
                }
                GridRow {
                    Text("Local TCP port").foregroundStyle(.secondary)
                    TextField("Local TCP port", text: $model.portText)
                        .textFieldStyle(.roundedBorder).frame(width: 100)
                        .accessibilityIdentifier("localTCPPort")
                        .help("An integer from 1024 to 65535. OBS must use the same port.")
                }
            }.disabled(model.active || model.isStopping)
            if !model.optionsAreValid {
                InlineNotice(text: "Enter a port from 1024 to 65535 using digits only.")
            }
            Divider()
            VStack(alignment: .leading, spacing: 5) {
                HStack {
                    Text("Bitrate").foregroundStyle(.secondary)
                    Spacer()
                    Text("\(model.options.bitrate / 1000) Mbps").monospacedDigit()
                        .accessibilityIdentifier("bitrateReadout")
                }
                Slider(value: Binding(get: { Double(model.options.bitrate) }, set: { model.options.bitrate = Int($0) }),
                       in: 2000...100000, step: 1000) { Text("Bitrate") }
                    .labelsHidden().accessibilityLabel("Bitrate")
                    .disabled(model.active || model.isStopping)
                Text("2–100 Mbps · Start with 15 Mbps on a reliable local network.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            if model.options.codec == .hdr {
                InlineNotice(text: "HDR requires HDR-capable PS5 content and a correctly configured OBS HDR workflow. No tone mapping or silent SDR fallback.", tone: .info)
            }
            if model.active || model.isStopping {
                Text("Stop capture to change stream settings.").font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}
