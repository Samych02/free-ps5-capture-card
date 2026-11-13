#pragma once
#include "CaptureEngine.h"
#include "LocalTCPServer.h"
#include <memory>
#include <functional>
namespace capture {
struct SessionSnapshot { capture_status status{}; bool done=false, retry=false; };
class SessionBackend {
public:
    virtual ~SessionBackend() = default;
    virtual SessionSnapshot snapshot() = 0;
    virtual void submitPin(const char *,size_t) = 0;
    virtual void stopAndJoin() = 0;
};
using SessionFactory=std::function<std::unique_ptr<SessionBackend>(const capture_options &, const capture_connection &, LocalTCPServer &)>;
std::unique_ptr<SessionBackend> makeLiveSession(const capture_options &,const capture_connection &,LocalTCPServer &);
}
