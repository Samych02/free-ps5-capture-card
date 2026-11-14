import Foundation

/// Current health only. Historical resets belong in diagnostics, never this banner.
struct CapturePresentation: Equatable {
    enum Tone { case neutral, info, success, warning, error }
    let title: String
    let detail: String
    let symbol: String
    let tone: Tone
    let isProgress: Bool

    init(state: EngineState, status: EngineStatus, isStopping: Bool = false) {
        let value: (String, String, String, Tone, Bool)
        if isStopping {
            value = ("Stopping capture…", "Disconnecting safely. Setup will unlock when the PS5 session has stopped.", "stop.circle", .info, true)
        } else {
            switch state {
            case .idle, .stopped:
                value = (state == .idle ? "Ready when you are" : "Capture stopped", "Choose your PS5 and stream settings, then start capture.", "record.circle", .neutral, false)
            case .connecting:
                value = ("Connecting to PS5…", "Establishing Remote Play. Keep your PS5 on and reachable on this network.", "network", .info, true)
            case .retrying:
                value = ("Reconnecting to PS5…", status.message.isEmpty ? "The source connection ended. Retrying automatically; you can stop at any time." : status.message, "arrow.trianglehead.2.clockwise.rotate.90", .warning, true)
            case .pinRequired:
                value = ("PS5 passcode required", status.message.isEmpty ? "Enter your console’s four-digit login passcode to continue. It is never saved." : status.message, "lock", .warning, false)
            case .failed, .unavailable:
                value = (state == .failed ? "Capture needs attention" : "Cannot start capture", status.message.isEmpty ? "Check your PS5, pairing and stream settings. Stop to reset an active capture before trying again." : status.message, "exclamationmark.triangle", .error, false)
            case .waitingOBS, .streaming:
                switch status.phase {
                case .protectedContent:
                    value = ("Protected content", "The PS5 cannot stream this content. Return to the home screen or a game; capture resumes when the console permits it.", "lock.shield", .warning, false)
                case .paused:
                    value = ("Waiting for PS5 media", "Remote Play is still connected. Waiting for fresh audio and video after a source pause.", "pause.circle", .warning, false)
                case .waitingMedia:
                    value = ("Receiving PS5 media…", "Waiting for usable video and audio before opening the OBS output.", "waveform", .info, true)
                case .startingOutput:
                    value = ("Starting OBS output…", "OBS is connected. Waiting for a fresh video frame and aligned audio.", "arrow.up.forward.video", .info, true)
                case .waitingOBS:
                    value = ("Waiting for OBS", "PS5 media is ready. Connect an OBS Media Source using the URL below.", "link", .info, false)
                case .streaming:
                    value = ("Streaming to OBS", "Sending compressed video and audio. OBS handles decoding and recording.", "checkmark.circle.fill", .success, false)
                case .none:
                    value = state == .streaming
                        ? ("Streaming to OBS", "Sending compressed video and audio. OBS handles decoding and recording.", "checkmark.circle.fill", .success, false)
                        : ("Preparing capture…", "Waiting for the PS5 media and local output to become ready.", "waveform", .info, true)
                }
            }
        }
        (title, detail, symbol, tone, isProgress) = value
    }
}
