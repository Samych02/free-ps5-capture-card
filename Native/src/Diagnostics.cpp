#include "Diagnostics.h"
#include <cstring>
#include <algorithm>
#include <limits>
extern "C" {
#include <libavutil/error.h>
}
namespace capture {
std::string outputQueueResetMessage(const PacketQueue::Stats &state) {
    using Stage=PacketQueue::MissingStage;
    if(state.reason!=PacketQueue::Reason::MissingTrack || state.missing.stage==Stage::None)
        return outputQueueResetMessage(state.reason);
    const auto &detail=state.missing;
    const char *cause=detail.stage==Stage::Configuration?"missing track configuration":
        detail.stage==Stage::StartupAlignment?"startup alignment failed":
        detail.stage==Stage::Audio?"audio delivery gap":
        detail.stage==Stage::Video?"video delivery gap":"both tracks delivery gap";
    std::string message="Last OBS reset: ";message+=cause;
    message+=" (audio age "+std::to_string(detail.audioAgeUs/1000)+" ms, video age "+std::to_string(detail.videoAgeUs/1000)+" ms";
    if(detail.stage==Stage::StartupAlignment && detail.audioStartDeltaUs)
        message+=", audio-start "+std::to_string(*detail.audioStartDeltaUs/1000)+" ms";
    return message+").";
}
const char *outputQueueResetMessage(PacketQueue::Reason reason) {
    switch(reason) {
        case PacketQueue::Reason::Overflow: return "Last OBS reset: output storage limit reached.";
        case PacketQueue::Reason::StaleMedia: return "Last OBS reset: queued media became stale.";
        case PacketQueue::Reason::MissingTrack: return "Last OBS reset: a required media track did not arrive in time.";
        case PacketQueue::Reason::KeyframeTimeout: return "Last OBS reset: no fresh keyframe before the startup deadline.";
        case PacketQueue::Reason::Timestamp: return "Last OBS reset: media timestamps were discontinuous.";
        case PacketQueue::Reason::InvalidData: return "Last OBS reset: invalid encoded packet metadata.";
        case PacketQueue::Reason::ConfigurationChanged: return "Last OBS reset: output configuration changed.";
        default: return "Last OBS reset: output session changed.";
    }
}
const char *outputWriteResetMessage(int error) {
    if(error==AVERROR(ETIMEDOUT)) return "Last OBS reset: local socket write stalled.";
    if(error==AVERROR(EPIPE) || error==AVERROR(ECONNRESET)) return "Last OBS reset: client closed the local connection.";
    if(error==AVERROR_EXIT) return "Last OBS reset: local socket write was cancelled.";
    return "Last OBS reset: local socket write failed.";
}
unsigned nextRetryAttempt(unsigned previous,uint64_t stableUs) {
    if(stableUs>=30000000) return 1;
    return previous==std::numeric_limits<unsigned>::max()?previous:previous+1;
}
unsigned retryDelayMilliseconds(unsigned attempt) {
    unsigned exponent=attempt?std::min(attempt-1,4u):0;
    return std::min(15u,1u<<exponent)*1000+(attempt*73)%200;
}
SessionSnapshot classifyQuit(ChiakiQuitReason reason) {
    SessionSnapshot s;s.done=true;s.retry=true;s.status.state=CAPTURE_FAILED;s.status.error=CAPTURE_ERROR_CONNECTION;
    const char *message="PS5 connection ended. Retrying; check power and local network access.";
    switch(reason) {
        // Fixed local categories only: never append core/remote reason strings,
        // HTTP requests or logs, which may contain authentication material.
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_UNKNOWN:
            message="PS5 session request failed (SESSION_REQUEST_UNKNOWN). Retrying.";break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_CONNECTION_REFUSED:
            message="PS5 session request connection refused (SESSION_REQUEST_CONNECTION_REFUSED). Retrying.";break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_CRASH:
            message="PS5 reported a Remote Play application failure (SESSION_REQUEST_RP_CRASH). Retrying.";break;
        case CHIAKI_QUIT_REASON_CTRL_UNKNOWN:
            message="PS5 control connection ended (CTRL_UNKNOWN). Retrying.";break;
        case CHIAKI_QUIT_REASON_CTRL_CONNECT_FAILED:
            message="PS5 control handshake failed (CTRL_CONNECT_FAILED). Retrying.";break;
        case CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN:
            message="PS5 media connection failed (STREAM_CONNECTION_UNKNOWN). Retrying.";break;
        case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED:
            message="PS5 ended the media stream (STREAM_CONNECTION_REMOTE_DISCONNECTED). Retrying.";break;
        case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_SHUTDOWN:
            message="PS5 ended the media stream for shutdown (STREAM_CONNECTION_REMOTE_SHUTDOWN). Retrying.";break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE:
            s.retry=false;s.status.error=CAPTURE_ERROR_IN_USE;message="PS5 reports Remote Play is already in use. Stop any other client, or wait for the previous session to close, then try again.";break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_VERSION_MISMATCH:
            s.retry=false;message="PS5 Remote Play protocol version is unsupported.";break;
        case CHIAKI_QUIT_REASON_PSN_REGIST_FAILED:
        case CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED:
            s.retry=false;s.status.error=CAPTURE_ERROR_IDENTITY;message="PS5 rejected the saved credentials. Check the account and pair again.";break;
        case CHIAKI_QUIT_REASON_STOPPED:
            s.retry=false;s.status.state=CAPTURE_STOPPED;s.status.error=CAPTURE_ERROR_NONE;message="";break;
        default:break;
    }
    strncpy(s.status.message,message,sizeof(s.status.message)-1);return s;
}
}
