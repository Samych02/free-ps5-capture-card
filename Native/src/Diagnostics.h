#pragma once
#include "Session.h"
#include <chiaki/session.h>
namespace capture {
SessionSnapshot classifyQuit(ChiakiQuitReason);
unsigned nextRetryAttempt(unsigned previous,uint64_t stableUs);
unsigned retryDelayMilliseconds(unsigned attempt);
}
