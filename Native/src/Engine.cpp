#include "CaptureEngine.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <new>

struct capture_engine {
    capture_options options;
    capture_callback callback;
    void *context;
    std::mutex mutex;
    std::condition_variable changed;
    bool closing = false;
    size_t stops = 0;
    std::thread worker;

    capture_engine(capture_options o, capture_callback cb, void *ctx)
        : options(o), callback(cb), context(ctx) {}
    void emit(capture_state state) { if(callback) callback(state, context); }
    void run() {
        emit(CAPTURE_IDLE);
        std::unique_lock<std::mutex> lock(mutex);
        while(true) {
            changed.wait(lock, [&] { return closing || stops; });
            if(closing) break;
            --stops;
            lock.unlock();
            emit(CAPTURE_STOPPED);
            lock.lock();
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
    auto result = capture_validate_options(o);
    if(error) *error = result;
    if(result != CAPTURE_OK) return nullptr;
    capture_engine *engine = nullptr;
    try {
        engine = new capture_engine(*o, cb, ctx);
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
    return engine ? CAPTURE_UNAVAILABLE : CAPTURE_INVALID_STATE;
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
