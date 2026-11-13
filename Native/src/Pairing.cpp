#include "SetupInternal.h"
#include <arpa/inet.h>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <algorithm>

static void clear_private(void *data, size_t count) {
    volatile unsigned char *p = static_cast<volatile unsigned char *>(data);
    while(count--) *p++ = 0;
}

capture_result capture_validate_pairing(const capture_pairing_options *o) {
    if(!o || o->size != sizeof(*o) || o->version != CAPTURE_API_VERSION)
        return CAPTURE_INVALID_OPTIONS;
    if(o->target != CHIAKI_TARGET_PS5_1) return CAPTURE_UNAVAILABLE;
    if(strnlen(o->pin, sizeof(o->pin)) != 8 ||
       !std::all_of(o->pin, o->pin + 8, [](char c) { return c >= '0' && c <= '9'; }) ||
       !std::any_of(o->account_id, o->account_id + 8, [](uint8_t b) { return b != 0; }))
        return CAPTURE_INVALID_OPTIONS;
    if(strnlen(o->address, sizeof(o->address)) == sizeof(o->address)) return CAPTURE_INVALID_OPTIONS;
    in_addr addr4{}; in6_addr addr6{};
    if(inet_pton(AF_INET, o->address, &addr4) == 1) {
        auto ip = ntohl(addr4.s_addr);
        if(ip == 0 || ip == 0xffffffff || (ip >> 24) == 127 || (ip >> 28) >= 14)
            return CAPTURE_INVALID_OPTIONS;
    } else if(inet_pton(AF_INET6, o->address, &addr6) != 1 ||
              IN6_IS_ADDR_UNSPECIFIED(&addr6) || IN6_IS_ADDR_LOOPBACK(&addr6) || IN6_IS_ADDR_MULTICAST(&addr6))
        return CAPTURE_INVALID_OPTIONS;
    return CAPTURE_OK;
}
bool copy_registration(const ChiakiRegisteredHost &h, capture_registration &out) {
    if(h.target != CHIAKI_TARGET_PS5_1 ||
       !std::any_of(h.server_mac, h.server_mac + 6, [](uint8_t b) { return b != 0; }) ||
       !std::any_of(h.rp_regist_key, h.rp_regist_key + 16, [](char b) { return b != 0; }) ||
       !std::any_of(h.rp_key, h.rp_key + 16, [](uint8_t b) { return b != 0; })) return false;
    out = {};
    out.target = h.target; out.key_type = h.rp_key_type;
    memcpy(out.mac, h.server_mac, 6);
    memcpy(out.name, h.server_nickname, strnlen(h.server_nickname, sizeof(h.server_nickname)));
    memcpy(out.registration_key, h.rp_regist_key, 16); memcpy(out.session_key, h.rp_key, 16);
    return true;
}
struct capture_pairing {
    capture_pairing_options options;
    PairingAPI api;
    capture_pairing_callback callback;
    void *context;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable changed;
    bool stopping = false, finished = false;
    capture_setup_status status = CAPTURE_SETUP_FAILED;
    capture_registration result{};
    ~capture_pairing() { clear_private(&options, sizeof(options)); clear_private(&result, sizeof(result)); }
    static void receive(ChiakiRegistEvent *event, void *ctx) {
        auto &self = *static_cast<capture_pairing *>(ctx);
        std::lock_guard<std::mutex> lock(self.mutex);
        if(self.finished) return;
        if(event->type == CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS && event->registered_host &&
           copy_registration(*event->registered_host, self.result)) self.status = CAPTURE_SETUP_OK;
        else if(event->type == CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED) self.status = CAPTURE_SETUP_CANCELLED;
        self.finished = true;
        self.changed.notify_one();
    }
    void run() {
        ChiakiRegist regist{}; ChiakiLog log{};
        // Registration debug logs can dump credentials; never forward raw logs.
        chiaki_log_init(&log, 0, nullptr, nullptr);
        ChiakiRegistInfo info{};
        info.target = static_cast<ChiakiTarget>(options.target); info.host = options.address;
        memcpy(info.psn_account_id, options.account_id, 8);
        for(size_t i = 0; i < 8; ++i) info.pin = info.pin * 10 + (options.pin[i] - '0');
        bool started = api.start(&regist, &log, &info, receive, this) == CHIAKI_ERR_SUCCESS;
        clear_private(&info, sizeof(info)); clear_private(&options, sizeof(options));
        if(started) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return stopping || finished; });
            }
            // The callback thread only copies/announces; joining is always here.
            api.stop(&regist); api.fini(&regist);
        }
        capture_setup_status finalStatus;
        { std::lock_guard<std::mutex> lock(mutex); finalStatus = stopping ? CAPTURE_SETUP_CANCELLED : status; }
        if(callback) callback(finalStatus, finalStatus == CAPTURE_SETUP_OK ? &result : nullptr, context);
        clear_private(&regist, sizeof(regist));
    }
};
capture_pairing *pairing_create(const capture_pairing_options *o, capture_pairing_callback cb,
    void *ctx, capture_result *err, PairingAPI api) {
    auto validation = capture_validate_pairing(o);
    if(err) *err = validation;
    if(validation != CAPTURE_OK) return nullptr;
    capture_pairing *handle = nullptr;
    try {
        handle = new capture_pairing{};
        handle->options = *o; handle->callback = cb; handle->context = ctx; handle->api = api;
        handle->worker = std::thread([handle] { handle->run(); });
        return handle;
    } catch(...) { delete handle; if(err) *err = CAPTURE_INTERNAL_ERROR; return nullptr; }
}
capture_pairing *capture_pairing_create(const capture_pairing_options *o, capture_pairing_callback cb,
    void *ctx, capture_result *err) {
    return pairing_create(o, cb, ctx, err, {chiaki_regist_start, chiaki_regist_stop, chiaki_regist_fini});
}
void capture_pairing_stop(capture_pairing *p) {
    if(!p) return;
    std::lock_guard<std::mutex> lock(p->mutex); p->stopping = true; p->changed.notify_one();
}
capture_result capture_pairing_destroy(capture_pairing *p) {
    if(!p) return CAPTURE_OK;
    if(p->worker.get_id() == std::this_thread::get_id()) return CAPTURE_INVALID_STATE;
    capture_pairing_stop(p); p->worker.join(); delete p; return CAPTURE_OK;
}
