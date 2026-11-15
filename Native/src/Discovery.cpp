#include "SetupInternal.h"
#include "platform/NetworkSetup.h"
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cerrno>

std::vector<capture_console> copy_consoles(ChiakiDiscoveryHost *hosts, size_t count) {
    std::vector<capture_console> result;
    for(size_t i = 0; hosts && i < count && i < 16; ++i) {
        auto &h = hosts[i];
        if(!chiaki_discovery_host_is_ps5(&h) || !h.host_addr || !h.host_id || strlen(h.host_id) != 12) continue;
        capture_console out{};
        if(strnlen(h.host_addr, sizeof(out.address)) >= sizeof(out.address)) continue;
        memcpy(out.address, h.host_addr, strlen(h.host_addr));
        memcpy(out.id, h.host_id, 12);
        if(h.host_name) memcpy(out.name, h.host_name, strnlen(h.host_name, sizeof(out.name) - 1));
        // This pinned core only supports the PS5 1.x registration target.
        out.target = CHIAKI_TARGET_PS5_1;
        out.state = h.state;
        result.push_back(out);
    }
    return result;
}
static thread_local capture_discovery *callbackOwner = nullptr;
struct capture_discovery {
    DiscoveryAPI api;
    capture_discovery_callback callback;
    void *context;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable changed;
    bool stopping = false;
    bool permissionReported = false;
    static void log_error(ChiakiLogLevel, const char *message, void *ctx) {
        // The pinned library reports sendto failures only through its logger.
        // Recognize explicit OS permission errors; never forward raw messages
        // or misclassify ordinary host-unreachable/timeouts as TCC denial.
        const char *prefix = "Discovery failed to send: ";
        if(strncmp(message, prefix, strlen(prefix))) return;
        const char *reason = message + strlen(prefix);
        if(strcmp(reason, strerror(EACCES)) && strcmp(reason, strerror(EPERM))) return;
        auto *self = static_cast<capture_discovery *>(ctx);
        {
            std::lock_guard<std::mutex> lock(self->mutex);
            if(self->permissionReported) return;
            self->permissionReported = true;
        }
        callbackOwner = self;
        if(self->callback) self->callback(CAPTURE_SETUP_PERMISSION_DENIED, nullptr, 0, self->context);
        callbackOwner = nullptr;
    }
    static void receive(ChiakiDiscoveryHost *hosts, size_t count, void *ctx) {
        auto *self = static_cast<capture_discovery *>(ctx);
        try {
            auto snapshot = copy_consoles(hosts, count);
            callbackOwner = self;
            if(self->callback) self->callback(CAPTURE_SETUP_OK, snapshot.data(), snapshot.size(), self->context);
            callbackOwner = nullptr;
        } catch(...) { callbackOwner = nullptr; }
    }
    void run() {
        ChiakiLog log{}; chiaki_log_init(&log, CHIAKI_LOG_ERROR, log_error, this);
        capture::platform::DiscoveryAddresses addresses;
        ChiakiDiscoveryServiceOptions options{};
        options.hosts_max = 16; options.host_drop_pings = 3; options.ping_ms = 500;
        addresses.apply(options);
        options.cb = receive; options.cb_user = this;
        ChiakiDiscoveryService service{};
        errno = 0;
        auto error = api.init(&service, &options, &log);
        if(error != CHIAKI_ERR_SUCCESS) {
            auto status = errno == EACCES || errno == EPERM ? CAPTURE_SETUP_PERMISSION_DENIED : CAPTURE_SETUP_FAILED;
            if(callback) callback(status, nullptr, 0, context);
            return;
        }
        { std::unique_lock<std::mutex> lock(mutex); changed.wait(lock, [&] { return stopping; }); }
        api.fini(&service);
    }
};
capture_discovery *discovery_create(capture_discovery_callback cb, void *ctx, capture_result *err, DiscoveryAPI api) {
    capture_discovery *handle = nullptr;
    try {
        handle = new capture_discovery{}; handle->callback = cb; handle->context = ctx;
        handle->api = api;
        handle->worker = std::thread([handle] {
            try { handle->run(); }
            catch(...) { if(handle->callback) handle->callback(CAPTURE_SETUP_FAILED, nullptr, 0, handle->context); }
        });
        if(err) *err = CAPTURE_OK;
        return handle;
    } catch(...) { delete handle; if(err) *err = CAPTURE_INTERNAL_ERROR; return nullptr; }
}
capture_discovery *capture_discovery_create(capture_discovery_callback cb, void *ctx, capture_result *err) {
    return discovery_create(cb, ctx, err, {chiaki_discovery_service_init, chiaki_discovery_service_fini});
}
void capture_discovery_stop(capture_discovery *p) {
    if(!p) return;
    std::lock_guard<std::mutex> lock(p->mutex); p->stopping = true; p->changed.notify_one();
}
capture_result capture_discovery_destroy(capture_discovery *p) {
    if(!p) return CAPTURE_OK;
    if(callbackOwner == p || p->worker.get_id() == std::this_thread::get_id()) return CAPTURE_INVALID_STATE;
    capture_discovery_stop(p); p->worker.join(); delete p; return CAPTURE_OK;
}
