#pragma once
#include "Session.h"
#include "PacketQueue.h"
#include <chiaki/session.h>
#include <string>
namespace capture {
SessionSnapshot classifyQuit(ChiakiQuitReason);
unsigned nextRetryAttempt(unsigned previous,uint64_t stableUs);
unsigned retryDelayMilliseconds(unsigned attempt);
const char *outputQueueResetMessage(PacketQueue::Reason);
std::string outputQueueResetMessage(const PacketQueue::Stats &);
const char *outputWriteResetMessage(int error);
}
