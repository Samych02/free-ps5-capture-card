#pragma once
#include "Session.h"
#include "PacketQueue.h"
#include <chiaki/session.h>
#include <string>
namespace capture {
class SessionReleaseGrace {
    std::optional<uint64_t> releasedUs_;
public:
    static constexpr uint64_t WindowUs=15000000;
    void established() {releasedUs_.reset();}
    void toreDown(bool establishedSession,bool automaticRetry,uint64_t now) {
        if(establishedSession && automaticRetry) releasedUs_=now;
    }
    bool allowsBusyRetry(uint64_t now) const {
        return releasedUs_ && now>=*releasedUs_ && now-*releasedUs_<WindowUs;
    }
};
SessionSnapshot classifyQuit(ChiakiQuitReason);
unsigned nextRetryAttempt(unsigned previous,uint64_t stableUs);
unsigned retryDelayMilliseconds(unsigned attempt);
const char *outputQueueResetMessage(PacketQueue::Reason);
std::string outputQueueResetMessage(const PacketQueue::Stats &);
const char *outputWriteResetMessage(int error);
}
