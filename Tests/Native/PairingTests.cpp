#include "SetupInternal.h"
#include <munit.h>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cerrno>

static capture_pairing_options options() {
    capture_pairing_options o{};
    o.size = sizeof(o); o.version = CAPTURE_API_VERSION; o.target = CHIAKI_TARGET_PS5_1;
    strcpy(o.address, "192.168.1.20"); strcpy(o.pin, "01234567"); o.account_id[0] = 42;
    return o;
}
static MunitResult validation(const MunitParameter[], void *) {
    auto o = options();
    munit_assert_int(capture_validate_pairing(&o), ==, CAPTURE_OK);
    munit_assert_int(capture_validate_pairing(nullptr), ==, CAPTURE_INVALID_OPTIONS);
    o.target = CHIAKI_TARGET_PS4_10;
    munit_assert_int(capture_validate_pairing(&o), ==, CAPTURE_UNAVAILABLE);
    for(auto pin : {"1234", "1234567", "123456789", "1234567x"}) {
        o = options(); memset(o.pin, 0, sizeof(o.pin)); memcpy(o.pin, pin, std::min(strlen(pin), sizeof(o.pin)));
        munit_assert_int(capture_validate_pairing(&o), ==, CAPTURE_INVALID_OPTIONS);
    }
    for(auto address : {"", "https://192.168.1.20", "192.168.1.20:9295", "localhost", "255.255.255.255"}) {
        o = options(); strcpy(o.address, address);
        munit_assert_int(capture_validate_pairing(&o), ==, CAPTURE_INVALID_OPTIONS);
    }
    o = options(); memset(o.account_id, 0, sizeof(o.account_id));
    munit_assert_int(capture_validate_pairing(&o), ==, CAPTURE_INVALID_OPTIONS);
    o = options(); o.version++;
    munit_assert_int(capture_validate_pairing(&o), ==, CAPTURE_INVALID_OPTIONS);
    return MUNIT_OK;
}
static MunitResult borrowed(const MunitParameter[], void *) {
    ChiakiRegisteredHost host{}; host.target = CHIAKI_TARGET_PS5_1;
    host.server_mac[0] = 7; strcpy(host.server_nickname, "PS5");
    memset(host.rp_regist_key, 3, 16); memset(host.rp_key, 4, 16); host.rp_key_type = 2;
    capture_registration copy{};
    munit_assert_true(copy_registration(host, copy));
    memset(&host, 0, sizeof(host));
    munit_assert_uint(copy.mac[0], ==, 7);
    munit_assert_string_equal(copy.name, "PS5");
    munit_assert_uint(copy.registration_key[15], ==, 3);
    munit_assert_uint(copy.session_key[15], ==, 4);
    munit_assert_false(copy_registration(host, copy));
    host.target = CHIAKI_TARGET_PS5_1; host.server_mac[0] = 1;
    munit_assert_false(copy_registration(host, copy)); // missing credential payload
    return MUNIT_OK;
}
static MunitResult snapshot(const MunitParameter[], void *) {
    char name[] = "Living Room";
    ChiakiDiscoveryHost hosts[2]{};
    hosts[0].host_type = "PS5"; hosts[0].host_name = name;
    hosts[0].device_discovery_protocol_version = CHIAKI_DISCOVERY_PROTOCOL_VERSION_PS5;
    hosts[0].host_addr = "192.168.1.20"; hosts[0].host_id = "012345abcdef";
    hosts[0].system_version = "01000000";
    hosts[0].state = CHIAKI_DISCOVERY_HOST_STATE_READY;
    hosts[1] = hosts[0]; hosts[1].device_discovery_protocol_version = CHIAKI_DISCOVERY_PROTOCOL_VERSION_PS4;
    auto copy = copy_consoles(hosts, 2);
    memset(name, 'x', sizeof(name) - 1);
    munit_assert_size(copy.size(), ==, 1);
    munit_assert_string_equal(copy[0].name, "Living Room");
    munit_assert_string_equal(copy[0].id, "012345abcdef");
    munit_assert_uint(copy[0].target, ==, CHIAKI_TARGET_PS5_1);
    munit_assert_size(copy_consoles(nullptr, 0).size(), ==, 0);
    return MUNIT_OK;
}
struct Fake {
    std::mutex mutex; std::condition_variable changed;
    bool started = false, joined = false; int stops = 0, callbacks = 0;
    uint32_t pin = 0; capture_setup_status status = CAPTURE_SETUP_OK;
    capture_registration result{};
    ChiakiRegistCb cb = nullptr; void *context = nullptr;
    bool succeed = false, failStart = false, failProtocol = false;
    std::thread::id finiThread;
};
static Fake *fake;
static ChiakiErrorCode start(ChiakiRegist *, ChiakiLog *, const ChiakiRegistInfo *info, ChiakiRegistCb cb, void *ctx) {
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->pin = info->pin; fake->cb = cb; fake->context = ctx;
        fake->started = true; fake->changed.notify_all();
    }
    if(fake->failStart) return CHIAKI_ERR_NETWORK;
    if(fake->succeed || fake->failProtocol) {
        ChiakiRegisteredHost host{}; host.target = CHIAKI_TARGET_PS5_1;
        host.server_mac[0] = 8; memset(host.rp_regist_key, 9, 16); memset(host.rp_key, 10, 16);
        ChiakiRegistEvent event{fake->failProtocol ? CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED : CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS, &host};
        cb(&event, ctx);
        memset(&host, 0, sizeof(host));
    }
    return CHIAKI_ERR_SUCCESS;
}
static void stop(ChiakiRegist *) { ++fake->stops; }
static void fini(ChiakiRegist *) { fake->joined = true; fake->finiThread = std::this_thread::get_id(); }
static void receive(capture_setup_status status, const capture_registration *registration, void *ctx) {
    auto &f = *static_cast<Fake *>(ctx);
    std::lock_guard<std::mutex> lock(f.mutex);
    f.status = status; if(registration) f.result = *registration;
    ++f.callbacks; f.changed.notify_all();
}
static MunitResult lifecycle(const MunitParameter[], void *) {
    for(int mode = 0; mode < 4; ++mode) {
        Fake f; fake = &f; f.succeed = mode == 1; f.failStart = mode == 2; f.failProtocol = mode == 3;
        auto o = options(); capture_result error = CAPTURE_OK;
        auto handle = pairing_create(&o, receive, &f, &error, {start, stop, fini});
        munit_assert_not_null(handle);
        memset(&o, 0, sizeof(o)); // caller memory must not be used by the worker
        {
            std::unique_lock<std::mutex> lock(f.mutex);
            munit_assert_true(f.changed.wait_for(lock, std::chrono::seconds(2), [&] { return f.started; }));
        }
        if(mode == 0) { capture_pairing_stop(handle); capture_pairing_stop(handle); }
        {
            std::unique_lock<std::mutex> lock(f.mutex);
            munit_assert_true(f.changed.wait_for(lock, std::chrono::seconds(2), [&] { return f.callbacks == 1; }));
        }
        munit_assert_int(capture_pairing_destroy(handle), ==, CAPTURE_OK);
        munit_assert_int(f.callbacks, ==, 1);
        munit_assert_uint(f.pin, ==, 1234567);
        munit_assert_int(f.status, ==, mode == 0 ? CAPTURE_SETUP_CANCELLED : mode == 1 ? CAPTURE_SETUP_OK : CAPTURE_SETUP_FAILED);
        munit_assert_int(f.joined, ==, mode != 2);
        if(f.joined) munit_assert_true(f.finiThread != std::this_thread::get_id());
        if(mode == 1) munit_assert_uint(f.result.session_key[0], ==, 10);
    }
    return MUNIT_OK;
}
struct DiscoveryFake {
    bool fail = false, denySend = false, joined = false;
    std::mutex mutex; std::condition_variable changed;
    int callbacks = 0;
    capture_setup_status status = CAPTURE_SETUP_OK;
    capture_result callbackDestroy = CAPTURE_OK;
};
static DiscoveryFake *discoveryFake;
static ChiakiErrorCode discovery_init(ChiakiDiscoveryService *, ChiakiDiscoveryServiceOptions *options, ChiakiLog *log) {
    if(discoveryFake->fail) { errno = EACCES; return CHIAKI_ERR_NETWORK; }
    if(discoveryFake->denySend) {
        chiaki_log(log, CHIAKI_LOG_ERROR, "Discovery failed to send: %s", strerror(EACCES));
        return CHIAKI_ERR_SUCCESS;
    }
    options->cb(nullptr, 0, options->cb_user);
    return CHIAKI_ERR_SUCCESS;
}
static void discovery_fini(ChiakiDiscoveryService *) { discoveryFake->joined = true; }
static MunitResult discovery_lifecycle(const MunitParameter[], void *) {
    for(int mode = 0; mode < 3; ++mode) {
        DiscoveryFake f; discoveryFake = &f; f.fail = mode == 1; f.denySend = mode == 2;
        capture_result error;
        auto *handle = discovery_create([](capture_setup_status status, const capture_console *, size_t count, void *ctx) {
            auto &f = *static_cast<DiscoveryFake *>(ctx);
            std::lock_guard<std::mutex> lock(f.mutex);
            f.status = status; ++f.callbacks;
            munit_assert_size(count, ==, 0);
            f.changed.notify_one();
        }, &f, &error, {discovery_init, discovery_fini});
        munit_assert_not_null(handle);
        {
            std::unique_lock<std::mutex> lock(f.mutex);
            munit_assert_true(f.changed.wait_for(lock, std::chrono::seconds(2), [&] { return f.callbacks == 1; }));
        }
        capture_discovery_stop(handle); capture_discovery_stop(handle);
        munit_assert_int(capture_discovery_destroy(handle), ==, CAPTURE_OK);
        munit_assert_int(f.joined, ==, !f.fail);
        munit_assert_int(f.status, ==, mode > 0 ? CAPTURE_SETUP_PERMISSION_DENIED : CAPTURE_SETUP_OK);
        munit_assert_int(f.callbacks, ==, 1);
    }
    return MUNIT_OK;
}
int main(int argc, char **argv) {
    MunitTest tests[] = {
        {const_cast<char *>("/validation"), validation, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/borrowed"), borrowed, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/snapshot"), snapshot, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/lifecycle"), lifecycle, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/discovery-lifecycle"), discovery_lifecycle, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr}};
    MunitSuite suite{const_cast<char *>("/pairing"), tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&suite, nullptr, argc, argv);
}
