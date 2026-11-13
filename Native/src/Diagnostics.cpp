#include "Diagnostics.h"
#include <cstring>
#include <algorithm>
#include <limits>
namespace capture {
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
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE:
            s.retry=false;s.status.error=CAPTURE_ERROR_IN_USE;message="Remote Play is already in use. Stop the other session and try again.";break;
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
