#pragma once
#include "Session.h"
#include <chiaki/session.h>
namespace capture {
struct LiveSessionAPI {
    decltype(&chiaki_session_init) init=chiaki_session_init;
    decltype(&chiaki_session_set_encoded_media_sink) sink=chiaki_session_set_encoded_media_sink;
    decltype(&chiaki_session_start) start=chiaki_session_start;
    decltype(&chiaki_session_stop) stop=chiaki_session_stop;
    decltype(&chiaki_session_join) join=chiaki_session_join;
    decltype(&chiaki_session_fini) fini=chiaki_session_fini;
    decltype(&chiaki_session_set_login_pin) pin=chiaki_session_set_login_pin;
};
std::unique_ptr<SessionBackend> makeLiveSessionWithAPI(const capture_options &,const capture_connection &,LocalTCPServer &,LiveSessionAPI);
}
