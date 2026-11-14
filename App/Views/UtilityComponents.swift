import SwiftUI

extension CapturePresentation.Tone {
    var color: Color {
        switch self {
        case .neutral: return .secondary
        case .info: return .blue
        case .success: return .green
        case .warning: return .orange
        case .error: return .red
        }
    }
}

struct UtilitySection<Content: View>: View {
    let title: String
    let symbol: String
    @ViewBuilder var content: () -> Content

    var body: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 14, content: content)
                .frame(maxWidth: .infinity, alignment: .leading).padding(8)
        } label: {
            Label(title, systemImage: symbol).font(.headline)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct InlineNotice: View {
    let text: String
    var tone: CapturePresentation.Tone = .error
    var body: some View {
        Label {
            Text(text).fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
        } icon: {
            Image(systemName: tone == .error ? "exclamationmark.circle" : "info.circle")
        }
        .font(.callout).foregroundStyle(tone.color)
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .combine)
    }
}

struct CaptureStatusView: View {
    let presentation: CapturePresentation
    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: presentation.symbol).font(.title2)
                .foregroundStyle(presentation.tone.color).frame(width: 30, height: 30)
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 5) {
                Text(presentation.title).font(.headline).accessibilityIdentifier("captureStatusTitle")
                Text(presentation.detail).font(.callout).foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
            }.frame(maxWidth: .infinity, alignment: .leading)
            if presentation.isProgress {
                ProgressView().controlSize(.small).padding(.top, 4)
                    .accessibilityLabel(presentation.title)
            }
        }
        .padding(16)
        .background(presentation.tone.color.opacity(0.075), in: RoundedRectangle(cornerRadius: 12))
        .accessibilityElement(children: .contain)
    }
}
