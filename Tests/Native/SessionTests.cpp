#include "EngineInternal.h"
#include "SessionInternal.h"
#include <munit.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <chrono>
using namespace capture;
struct Events {
    std::mutex mutex; std::condition_variable changed;
    capture_state state=CAPTURE_IDLE;
    std::atomic<int> starts{0},joins{0},pins{0};
    std::atomic<bool> requestPin{false};
};
struct FakeSession: SessionBackend {
    Events &events; bool joined=false;
    explicit FakeSession(Events &e):events(e) { ++e.starts; }
    ~FakeSession() { stopAndJoin(); }
    SessionSnapshot snapshot() override {
        SessionSnapshot s; s.status.state=events.requestPin?CAPTURE_PIN_REQUIRED:CAPTURE_WAITING_OBS; return s;
    }
    void submitPin(const char *,size_t n) override { if(n==4) { ++events.pins; events.requestPin=false; } }
    void stopAndJoin() override { if(!joined) { joined=true; ++events.joins; } }
};
static uint16_t availablePort() {
    int fd=socket(AF_INET,SOCK_STREAM,0); sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    munit_assert_int(bind(fd,reinterpret_cast<sockaddr *>(&address),sizeof(address)),==,0);
    socklen_t length=sizeof(address); getsockname(fd,reinterpret_cast<sockaddr *>(&address),&length);
    close(fd); return ntohs(address.sin_port);
}
static capture_connection connection() {
    capture_connection c{}; c.size=sizeof(c); c.version=CAPTURE_API_VERSION; strcpy(c.address,"192.168.1.20"); c.account_id[0]=42;
    c.registration.target=1000100; c.registration.mac[0]=1;
    memset(c.registration.registration_key,2,16); memset(c.registration.session_key,3,16); return c;
}
static capture_engine *create(capture_options o,Events &events) {
    capture_result result;
    return capture_engine_create_with_factory(&o,[](capture_state state,void *ctx) {
        auto &e=*static_cast<Events *>(ctx); std::lock_guard<std::mutex> lock(e.mutex); e.state=state; e.changed.notify_all();
    },&events,&result,[&](const auto &,const auto &,auto &) { return std::make_unique<FakeSession>(events); });
}
static bool wait(Events &e,capture_state state) {
    std::unique_lock<std::mutex> lock(e.mutex);
    return e.changed.wait_for(lock,std::chrono::seconds(3),[&] { return e.state==state; });
}
static MunitResult lifecycle(const MunitParameter[],void *) {
    Events e; auto options=capture_default_options(); options.port=availablePort(); auto engine=create(options,e);
    munit_assert_int(capture_engine_start(engine),==,CAPTURE_UNAVAILABLE);
    auto c=connection(); munit_assert_int(capture_engine_configure(engine,&options,&c),==,CAPTURE_OK);
    munit_assert_int(capture_engine_start(engine),==,CAPTURE_OK); munit_assert_true(wait(e,CAPTURE_WAITING_OBS));
    munit_assert_int(capture_engine_configure(engine,&options,&c),==,CAPTURE_INVALID_STATE);
    capture_engine_stop(engine); munit_assert_true(wait(e,CAPTURE_STOPPED));
    munit_assert_int(e.starts.load(),==,1); munit_assert_int(e.joins.load(),==,1);
    munit_assert_int(capture_engine_destroy(engine),==,CAPTURE_OK);
    return MUNIT_OK;
}
static MunitResult port_before_console(const MunitParameter[],void *) {
    Events e; auto options=capture_default_options(); options.port=availablePort(); LocalTCPServer occupied(uint16_t(options.port));
    auto engine=create(options,e); auto c=connection();
    munit_assert_int(capture_engine_configure(engine,&options,&c),==,CAPTURE_OK);
    capture_engine_start(engine); munit_assert_true(wait(e,CAPTURE_FAILED));
    munit_assert_int(e.starts.load(),==,0);
    capture_status status{}; status.size=sizeof(status); status.version=CAPTURE_API_VERSION;
    munit_assert_int(capture_engine_get_status(engine,&status),==,CAPTURE_OK); munit_assert_int(status.error,==,CAPTURE_ERROR_PORT);
    capture_engine_destroy(engine); return MUNIT_OK;
}
static MunitResult pin_and_identity(const MunitParameter[],void *) {
    Events e; auto options=capture_default_options(); options.port=availablePort(); auto engine=create(options,e); auto c=connection();
    c.registration.target=0;
    munit_assert_int(capture_engine_configure(engine,&options,&c),==,CAPTURE_INVALID_OPTIONS);
    c=connection(); munit_assert_int(capture_engine_configure(engine,&options,&c),==,CAPTURE_OK);
    munit_assert_int(capture_engine_submit_login_pin(engine,"1234",4),==,CAPTURE_INVALID_STATE);
    e.requestPin=true; capture_engine_start(engine); munit_assert_true(wait(e,CAPTURE_PIN_REQUIRED));
    munit_assert_int(capture_engine_submit_login_pin(engine,"12345678",8),==,CAPTURE_INVALID_OPTIONS);
    munit_assert_int(capture_engine_submit_login_pin(engine,"0123",4),==,CAPTURE_OK);
    munit_assert_true(wait(e,CAPTURE_WAITING_OBS)); munit_assert_int(e.pins.load(),==,1);
    capture_engine_destroy(engine); munit_assert_int(e.joins.load(),==,1); return MUNIT_OK;
}
static int nativeInit=0,nativeStart=0,nativeStop=0,nativeJoin=0,nativeFini=0,nativePin=0;
static bool failStart=false;
static ChiakiSession *borrowedSession=nullptr;
static MunitResult native_lifecycle(const MunitParameter[],void *) {
    LiveSessionAPI api;
    api.init=[](ChiakiSession *s,ChiakiConnectInfo *info,ChiakiLog *log) {
        ++nativeInit; borrowedSession=s;
        munit_assert_true(info->ps5); munit_assert_false(info->video_profile_auto_downgrade);
        munit_assert_false(info->enable_keyboard); munit_assert_false(info->enable_dualsense);
        munit_assert_int(info->video_profile.codec,==,CHIAKI_CODEC_H265_HDR);
        munit_assert_uint(info->video_profile.bitrate,==,15000);
        munit_assert_int(log->level_mask,==,0);
        return CHIAKI_ERR_SUCCESS;
    };
    api.sink=[](ChiakiSession *s,const ChiakiEncodedMediaSink *sink) {
        munit_assert_true(sink->video_packet!=nullptr); munit_assert_true(sink->audio_packet!=nullptr);
        s->encoded_sink=*sink; return CHIAKI_ERR_SUCCESS;
    };
    api.start=[](ChiakiSession *s) {
        ++nativeStart; if(failStart) return CHIAKI_ERR_UNKNOWN;
        ChiakiEvent e{};e.type=CHIAKI_EVENT_CONNECTED;s->event_cb(&e,s->event_cb_user);return CHIAKI_ERR_SUCCESS;
    };
    api.stop=[](ChiakiSession *){++nativeStop;return CHIAKI_ERR_SUCCESS;};
    api.join=[](ChiakiSession *){++nativeJoin;return CHIAKI_ERR_SUCCESS;};
    api.fini=[](ChiakiSession *){++nativeFini;};
    api.pin=[](ChiakiSession *,const uint8_t *p,size_t n){munit_assert_size(n,==,4);munit_assert_int(p[0],==,'0');++nativePin;return CHIAKI_ERR_SUCCESS;};
    auto o=capture_default_options();o.codec=CAPTURE_H265_HDR;auto c=connection();LocalTCPServer server(availablePort());
    auto session=makeLiveSessionWithAPI(o,c,server,api);
    munit_assert_int(session->snapshot().status.state,==,CAPTURE_WAITING_OBS);
    ChiakiEvent event{};event.type=CHIAKI_EVENT_LOGIN_PIN_REQUEST;
    borrowedSession->event_cb(&event,borrowedSession->event_cb_user);
    munit_assert_int(session->snapshot().status.state,==,CAPTURE_PIN_REQUIRED);
    session->submitPin("0123",4);munit_assert_int(nativePin,==,1);
    event.type=CHIAKI_EVENT_QUIT;event.quit.reason=CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE;
    borrowedSession->event_cb(&event,borrowedSession->event_cb_user);
    auto snapshot=session->snapshot();munit_assert_true(snapshot.done);munit_assert_false(snapshot.retry);
    munit_assert_int(snapshot.status.error,==,CAPTURE_ERROR_IN_USE);
    session->stopAndJoin();session.reset();
    munit_assert_int(nativeStop,==,1);munit_assert_int(nativeJoin,==,1);munit_assert_int(nativeFini,==,1);
    failStart=true;bool rejected=false;
    try {makeLiveSessionWithAPI(o,c,server,api);}catch(...){rejected=true;}
    failStart=false;munit_assert_true(rejected);
    munit_assert_int(nativeFini,==,2);munit_assert_int(nativeJoin,==,1);
    return MUNIT_OK;
}
int main(int argc,char **argv) {
    MunitTest tests[]{
        {const_cast<char *>("/lifecycle"),lifecycle,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/port-before-console"),port_before_console,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/pin-identity"),pin_and_identity,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/native-lifecycle"),native_lifecycle,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {nullptr,nullptr,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr}};
    MunitSuite suite{const_cast<char *>("/session"),tests,nullptr,1,MUNIT_SUITE_OPTION_NONE}; return munit_suite_main(&suite,nullptr,argc,argv);
}
