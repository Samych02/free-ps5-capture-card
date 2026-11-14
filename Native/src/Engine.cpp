#include "CaptureEngine.h"
#include "EngineInternal.h"
#include "Diagnostics.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <new>
#include <cstring>
#include <algorithm>
#include <array>
#include <chrono>

static void erase_private(void *p,size_t n) { auto b=static_cast<volatile unsigned char *>(p); while(n--) *b++=0; }

struct capture_engine {
    capture_options options;
    capture_connection connection{};
    capture_status status{};
    capture::SessionFactory factory;
    capture_callback callback;
    void *context;
    std::mutex mutex;
    std::condition_variable changed;
    bool closing = false;
    size_t stops = 0;
    bool configured=false, active=false, starting=false, pinPending=false;
    std::array<char,4> pin{};
    std::thread worker;

    capture_engine(capture_options o, capture_callback cb, void *ctx,capture::SessionFactory f)
        : options(o), factory(std::move(f)), callback(cb), context(ctx) { status.size=sizeof(status); status.version=CAPTURE_API_VERSION; status.port=o.port; }
    ~capture_engine() { erase_private(&connection,sizeof(connection)); erase_private(pin.data(),pin.size()); }
    void publish(capture_status s) {
        { std::lock_guard<std::mutex> lock(mutex); s.size=sizeof(s); s.version=CAPTURE_API_VERSION; s.port=options.port; status=s; }
        if(callback) callback(s.state,context);
    }
    void emit(capture_state state) {
        capture_status s{}; { std::lock_guard<std::mutex> lock(mutex); s=status; }
        s.state=state; if(state==CAPTURE_STOPPED || state==CAPTURE_IDLE) { s.error=CAPTURE_ERROR_NONE; s.message[0]=0; }
        publish(s);
    }
    bool cancelled() { std::lock_guard<std::mutex> lock(mutex); return closing || stops; }
    void failure(capture_error error,const char *message) {
        capture_status s{}; s.state=CAPTURE_FAILED; s.error=error; strncpy(s.message,message,sizeof(s.message)-1); publish(s);
    }
    void capture() {
        std::unique_ptr<capture::LocalTCPServer> server;
        try { server=std::make_unique<capture::LocalTCPServer>(uint16_t(options.port)); }
        catch(...) { failure(CAPTURE_ERROR_PORT,"Local output port is unavailable. Stop, choose a free port, and update OBS."); return; }
        unsigned retries=0;
        capture::SessionReleaseGrace releaseGrace;
        const auto captureOrigin=std::chrono::steady_clock::now();
        auto elapsedUs=[&](auto now) {return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(now-captureOrigin).count());};
        while(!cancelled()) {
            emit(CAPTURE_CONNECTING);
            bool retry=false;
            bool hadEstablishedSession=false,busyReleaseRetry=false;
            uint64_t stableUs=0;
            try {
                auto session=factory(options,connection,*server);
                auto last=std::chrono::steady_clock::now()-std::chrono::seconds(1);
                capture_state previous=CAPTURE_IDLE;
                auto stableSince=last;bool streaming=false;
                while(!cancelled()) {
                    std::array<char,4> pending{}; bool submit=false;
                    { std::lock_guard<std::mutex> lock(mutex); if(pinPending) { pending=pin; pinPending=false; erase_private(pin.data(),pin.size()); submit=true; } }
                    if(submit) {
                        try {session->submitPin(pending.data(),pending.size());}
                        catch(...) {erase_private(pending.data(),pending.size());throw;}
                        erase_private(pending.data(),pending.size());
                    }
                    auto snapshot=session->snapshot(); snapshot.status.reconnects=retries;
                    auto now=std::chrono::steady_clock::now();
                    if(!snapshot.done && (snapshot.status.state==CAPTURE_WAITING_OBS || snapshot.status.state==CAPTURE_STREAMING)) {
                        hadEstablishedSession=true;releaseGrace.established();
                    }
                    if(snapshot.done && snapshot.status.error==CAPTURE_ERROR_IN_USE && releaseGrace.allowsBusyRetry(elapsedUs(now))) {
                        snapshot.retry=true;snapshot.status.state=CAPTURE_RETRYING;busyReleaseRetry=true;
                        strncpy(snapshot.status.message,"PS5 is busy after the previous session ended. Retrying briefly.",sizeof(snapshot.status.message)-1);
                    }
                    if(snapshot.status.state==CAPTURE_STREAMING) {
                        if(!streaming) {streaming=true;stableSince=now;}
                        stableUs=std::chrono::duration_cast<std::chrono::microseconds>(now-stableSince).count();
                    } else {streaming=false;}
                    if(snapshot.status.state!=previous || now-last>=std::chrono::milliseconds(250)) {
                        publish(snapshot.status); previous=snapshot.status.state; last=now;
                    }
                    if(snapshot.done) { retry=snapshot.retry; break; }
                    std::unique_lock<std::mutex> lock(mutex);
                    changed.wait_for(lock,std::chrono::milliseconds(20),[&] { return closing || stops || pinPending; });
                }
                session->stopAndJoin();
            } catch(...) { failure(CAPTURE_ERROR_CONNECTION,"Cannot establish the PS5 connection. Check power, Remote Play, network permissions and pairing."); retry=true; }
            // Arm only after a previously established attempt is fully torn
            // down. Busy/failed replacement attempts must not extend the grace.
            releaseGrace.toreDown(hadEstablishedSession,retry,elapsedUs(std::chrono::steady_clock::now()));
            server->disconnect();
            if(cancelled() || !retry) break;
            retries=capture::nextRetryAttempt(retries,stableUs);emit(CAPTURE_RETRYING);
            std::unique_lock<std::mutex> lock(mutex);
            auto delay=capture::retryDelayMilliseconds(retries);
            if(busyReleaseRetry) delay=std::min(delay,2000u);
            changed.wait_for(lock,std::chrono::milliseconds(delay),[&] { return closing || stops; });
        }
    }
    void run() {
        emit(CAPTURE_IDLE);
        std::unique_lock<std::mutex> lock(mutex);
        while(true) {
            changed.wait(lock, [&] { return closing || stops || starting; });
            if(closing) break;
            if(stops) {
                --stops; starting=false; active=false; pinPending=false; erase_private(pin.data(),pin.size());
                erase_private(&connection,sizeof(connection));configured=false;
                lock.unlock(); emit(CAPTURE_STOPPED); lock.lock();
            } else {
                starting=false; lock.unlock(); capture(); lock.lock(); active=false;
            }
        }
    }
};
capture_options capture_default_options() {
    return {sizeof(capture_options), CAPTURE_API_VERSION, 1920, 1080, 60, 15000, 9295, CAPTURE_H265};
}
capture_result capture_validate_options(const capture_options *o) {
    if(!o || o->size != sizeof(*o) || o->version != CAPTURE_API_VERSION)
        return CAPTURE_INVALID_OPTIONS;
    bool resolution = (o->width == 1920 && o->height == 1080) ||
                      (o->width == 1280 && o->height == 720);
    if(!resolution || (o->fps != 30 && o->fps != 60) ||
       o->bitrate_kbps < 2000 || o->bitrate_kbps > 100000 ||
       o->port < 1024 || o->port > 65535 ||
       (o->codec != CAPTURE_H264 && o->codec != CAPTURE_H265 && o->codec != CAPTURE_H265_HDR))
        return CAPTURE_INVALID_OPTIONS;
    return CAPTURE_OK;
}
capture_engine *capture_engine_create(const capture_options *o, capture_callback cb, void *ctx, capture_result *error) {
    return capture_engine_create_with_factory(o,cb,ctx,error,capture::makeLiveSession);
}
capture_engine *capture_engine_create_with_factory(const capture_options *o,capture_callback cb,void *ctx,capture_result *error,capture::SessionFactory factory) {
    auto result = capture_validate_options(o);
    if(error) *error = result;
    if(result != CAPTURE_OK) return nullptr;
    capture_engine *engine = nullptr;
    try {
        engine = new capture_engine(*o, cb, ctx,std::move(factory));
        engine->worker = std::thread([engine] { engine->run(); });
        return engine;
    } catch(...) {
        delete engine;
        if(error) *error = CAPTURE_INTERNAL_ERROR;
        return nullptr;
    }
}
capture_result capture_engine_stop(capture_engine *engine) {
    if(!engine) return CAPTURE_INVALID_STATE;
    std::lock_guard<std::mutex> lock(engine->mutex);
    if(engine->closing) return CAPTURE_INVALID_STATE;
    // Stop is state-idempotent; callers still receive an acknowledgement.
    // Coalesce pathological request floods to keep control storage bounded.
    if(engine->stops < 256) ++engine->stops;
    engine->changed.notify_one();
    return CAPTURE_OK;
}
capture_result capture_engine_start(capture_engine *engine) {
    if(!engine) return CAPTURE_INVALID_STATE;
    std::lock_guard<std::mutex> lock(engine->mutex);
    if(engine->closing || engine->active || engine->starting || engine->stops) return CAPTURE_INVALID_STATE;
    if(!engine->configured) return CAPTURE_UNAVAILABLE;
    engine->starting=true; engine->active=true; engine->changed.notify_one(); return CAPTURE_OK;
}
capture_result capture_engine_destroy(capture_engine *engine) {
    if(!engine) return CAPTURE_OK;
    if(engine->worker.get_id() == std::this_thread::get_id()) return CAPTURE_INVALID_STATE;
    {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->closing = true;
        engine->changed.notify_one();
    }
    engine->worker.join();
    delete engine;
    return CAPTURE_OK;
}
capture_result capture_engine_configure(capture_engine *e,const capture_options *o,const capture_connection *c) {
    if(!e) return CAPTURE_INVALID_STATE;
    if(capture_validate_options(o)!=CAPTURE_OK || !c || c->size!=sizeof(*c) || c->version!=CAPTURE_API_VERSION) return CAPTURE_INVALID_OPTIONS;
    capture_pairing_options test{}; test.size=sizeof(test); test.version=CAPTURE_API_VERSION; test.target=c->registration.target;
    memcpy(test.address,c->address,sizeof(test.address)); memcpy(test.account_id,c->account_id,8); memcpy(test.pin,"00000000",9);
    if(capture_validate_pairing(&test)!=CAPTURE_OK ||
       !std::any_of(c->registration.mac,c->registration.mac+6,[](uint8_t b){return b;}) ||
       !std::any_of(c->registration.registration_key,c->registration.registration_key+16,[](uint8_t b){return b;}) ||
       !std::any_of(c->registration.session_key,c->registration.session_key+16,[](uint8_t b){return b;})) return CAPTURE_INVALID_OPTIONS;
    std::lock_guard<std::mutex> lock(e->mutex);
    if(e->active || e->starting || e->closing || e->stops) return CAPTURE_INVALID_STATE;
    e->options=*o; e->connection=*c; e->configured=true; return CAPTURE_OK;
}
capture_result capture_engine_get_status(capture_engine *e,capture_status *s) {
    if(!e) return CAPTURE_INVALID_STATE;
    if(!s || s->size!=sizeof(*s) || s->version!=CAPTURE_API_VERSION) return CAPTURE_INVALID_OPTIONS;
    std::lock_guard<std::mutex> lock(e->mutex); *s=e->status; return CAPTURE_OK;
}
capture_result capture_engine_submit_login_pin(capture_engine *e,const char *pin,size_t size) {
    if(!e) return CAPTURE_INVALID_STATE;
    if(!pin || size!=4 || !std::all_of(pin,pin+size,[](char c){return c>='0' && c<='9';})) return CAPTURE_INVALID_OPTIONS;
    std::lock_guard<std::mutex> lock(e->mutex);
    if(e->status.state!=CAPTURE_PIN_REQUIRED || !e->active || e->pinPending) return CAPTURE_INVALID_STATE;
    memcpy(e->pin.data(),pin,4); e->pinPending=true; e->changed.notify_one(); return CAPTURE_OK;
}
