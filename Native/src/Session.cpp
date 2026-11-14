#include "SessionInternal.h"
#include "Pipeline.h"
#include "Diagnostics.h"
#include <cstring>
#include <stdexcept>
namespace capture {
ChiakiErrorCode initializeCaptureCore() {
    // Process-wide FEC tables and certificate-cache mutex must exist before
    // any live session. C++ local-static initialization serializes concurrent
    // engine instances; chiaki_lib_init itself is not a repeatable per-session API.
    static const ChiakiErrorCode result=chiaki_lib_init();
    return result;
}
namespace {
void wipe(void *p,size_t n) {auto b=static_cast<volatile uint8_t *>(p);while(n--) *b++=0;}
class LiveSession final:public SessionBackend {
    ChiakiSession session_{};
    ChiakiLog log_{};
    LiveSessionAPI api_;
    bool initialized_=false,started_=false;
    std::mutex mutex_;
    bool connected_=false,pinRequired_=false,pinIncorrect_=false,done_=false;
    ChiakiQuitReason quit_=CHIAKI_QUIT_REASON_NONE;
    std::unique_ptr<SessionMediaControl> control_;
    std::unique_ptr<MediaPipeline> pipeline_;
    static void event(ChiakiEvent *e,void *ctx) {
        auto &self=*static_cast<LiveSession *>(ctx);
        std::lock_guard<std::mutex> lock(self.mutex_);
        switch(e->type) {
            case CHIAKI_EVENT_CONNECTED:self.connected_=true;self.pinRequired_=false;break;
            case CHIAKI_EVENT_LOGIN_PIN_REQUEST:self.pinRequired_=true;self.pinIncorrect_=e->login_pin_request.pin_incorrect;break;
            case CHIAKI_EVENT_QUIT:self.done_=true;self.quit_=e->quit.reason;break;
            default:break; // Never retain remote strings, account data or controller events.
        }
    }
public:
    explicit LiveSession(LiveSessionAPI api):api_(api) {}
    void start(const capture_options &o,const capture_connection &c,LocalTCPServer &server) {
        if(api_.libraryInit()!=CHIAKI_ERR_SUCCESS) throw std::runtime_error("Capture core initialization failed");
        // Raw core logs can contain keys, tokens and PINs. No logging callback.
        chiaki_log_init(&log_,0,nullptr,nullptr);
        ChiakiConnectInfo info{};
        info.ps5=true;info.host=c.address;
        memcpy(info.regist_key,c.registration.registration_key,16);
        memcpy(info.morning,c.registration.session_key,16);
        memcpy(info.psn_account_id,c.account_id,8);
        info.video_profile={o.width,o.height,o.fps,o.bitrate_kbps,static_cast<ChiakiCodec>(o.codec)};
        info.video_profile_auto_downgrade=false;
        info.packet_loss_max=0.05;info.enable_idr_on_fec_failure=true;
        auto result=api_.init(&session_,&info,&log_);wipe(&info,sizeof(info));
        if(result!=CHIAKI_ERR_SUCCESS) throw std::runtime_error("PS5 session initialization failed");
        initialized_=true;
        control_=std::make_unique<SessionMediaControl>(&session_,api_.stop);
        pipeline_=std::make_unique<MediaPipeline>(o,server,[this]{control_->requestKeyframe();});
        auto sink=pipeline_->sink();
        if(api_.sink(&session_,&sink)!=CHIAKI_ERR_SUCCESS) throw std::runtime_error("Encoded sink setup failed");
        // The core calls this from both RP-Prohibit connect handling and live
        // display restriction messages. Install before start; context remains
        // alive until core threads have joined, before pipeline destruction.
        ChiakiCtrlDisplaySink display{pipeline_.get(),[](void *context,bool blocked) {
            static_cast<MediaPipeline *>(context)->setSourceBlocked(blocked);
        }};
        chiaki_session_ctrl_set_display_sink(&session_,&display);
        chiaki_session_set_event_cb(&session_,event,this);
        if(api_.start(&session_)!=CHIAKI_ERR_SUCCESS) throw std::runtime_error("PS5 session start failed");
        started_=true;
    }
    ~LiveSession() override {stopAndJoin();wipe(&session_,sizeof(session_));}
    SessionSnapshot snapshot() override {
        auto s=pipeline_->snapshot();
        std::lock_guard<std::mutex> lock(mutex_);
        if(s.done) return s;
        if(done_) {
            auto result=classifyQuit(quit_);s.done=result.done;s.retry=result.retry;
            s.status.state=result.status.state;s.status.error=result.status.error;
            memcpy(s.status.message,result.status.message,sizeof(s.status.message));
        } else if(pinRequired_) {
            s.status.state=CAPTURE_PIN_REQUIRED;
            if(pinIncorrect_) strncpy(s.status.message,"PS5 login passcode was incorrect. Try again.",sizeof(s.status.message)-1);
        }
        else if(!connected_) s.status.state=CAPTURE_CONNECTING;
        return s;
    }
    void submitPin(const char *p,size_t n) override {
        {std::lock_guard<std::mutex> lock(mutex_);if(!pinRequired_ || done_ || n!=4)return;pinRequired_=false;}
        if(api_.pin(&session_,reinterpret_cast<const uint8_t *>(p),n)!=CHIAKI_ERR_SUCCESS)
            throw std::runtime_error("Passcode submission failed");
    }
    void stopAndJoin() override {
        // Close IDR access first. Join core receive threads before destroying
        // borrowed callback context, and never join from a receive callback.
        if(started_) {
            control_->stopAndDetach();
            api_.join(&session_);started_=false;
        }
        if(pipeline_) pipeline_->stopAndJoin();
        pipeline_.reset();control_.reset();
        if(initialized_) {api_.fini(&session_);initialized_=false;wipe(&session_,sizeof(session_));}
    }
};
}
std::unique_ptr<SessionBackend> makeLiveSessionWithAPI(const capture_options &o,const capture_connection &c,LocalTCPServer &s,LiveSessionAPI api) {
    auto session=std::make_unique<LiveSession>(api);session->start(o,c,s);return session;
}
std::unique_ptr<SessionBackend> makeLiveSession(const capture_options &o,const capture_connection &c,LocalTCPServer &s) {
    return makeLiveSessionWithAPI(o,c,s,{});
}
}
