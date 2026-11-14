import SwiftUI

struct OutputMetricsView: View {
    let status: EngineStatus

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if status.width > 0 {
                metric("Received format", "\(status.width) × \(status.height) · nominal \(cadence) fps")
                    .accessibilityIdentifier("receivedMediaStatus")
                metric("Media packets", "Video \(status.videoPackets) · Audio \(status.audioPackets)")
                metric("App queue", "\(status.queuedBytes / 1024) KiB · \(status.queueAgeMicroseconds / 1000) ms")
                metric("Missing media", "Video \(status.lostVideoFrames) · Audio \(status.missingAudioPackets)")
                metric("Source retry attempt", "\(status.reconnects)")
                    .help("Backoff attempt, not a total disconnect count. Restarts after a stable source session.")
                metric("OBS output resets", "\(status.outputDisconnects)")
                if status.videoClockElapsedMicroseconds > 0 {
                    metric("Counter drift", "Video \(status.videoClockDriftMicroseconds / 1000) ms · Audio \(status.audioClockDriftMicroseconds / 1000) ms")
                        .accessibilityIdentifier("sourceClockDrift")
                    metric("Observed counters", "Video \(rate(status.videoClockPosition, status.videoClockElapsedMicroseconds))/s · Audio \(rate(status.audioClockPosition, status.audioClockElapsedMicroseconds))/s")
                        .accessibilityIdentifier("sourceClockRates")
                    metric("Audio packet", "\(status.audioSamplesPerPacket) samples")
                }
                Text("Counter drift compares nominal source counters with arrival time. It is not measured A/V sync or total capture latency. Clock spans restart after a long media pause.")
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            } else {
                Text("Metrics appear when media arrives.").font(.callout).foregroundStyle(.secondary)
            }
            if !status.lastOutputMessage.isEmpty || !status.lastSourceMessage.isEmpty {
                Divider().padding(.vertical, 2)
                Text("Recent events").font(.subheadline.weight(.semibold))
                Text("History from this capture—not a current error.").font(.caption).foregroundStyle(.secondary)
                if !status.lastOutputMessage.isEmpty { history(status.lastOutputMessage) }
                if !status.lastSourceMessage.isEmpty { history("Last source retry: " + status.lastSourceMessage) }
            }
        }
        .textSelection(.enabled)
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.top, 8)
        .accessibilityIdentifier("outputMetrics")
    }
    private func metric(_ label: String, _ value: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(label).foregroundStyle(.secondary).frame(width: 132, alignment: .leading)
            Text(value).monospacedDigit().fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading)
        }.font(.caption).accessibilityElement(children: .combine)
    }
    private func history(_ value: String) -> some View {
        Text(value).font(.caption).foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true).frame(maxWidth: .infinity, alignment: .leading)
    }
    private var cadence: String {
        guard status.fpsDenominator > 0 else { return "unknown" }
        return String(format: "%.2f", Double(status.fpsNumerator) / Double(status.fpsDenominator))
    }
    private func rate(_ position: UInt64, _ elapsed: UInt64) -> String {
        guard elapsed > 0 else { return "unknown" }
        return String(format: "%.3f", Double(position) * 1_000_000 / Double(elapsed))
    }
}
