#include "CaptureEngine.h"
#include <munit.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>

static MunitResult defaults(const MunitParameter[], void *) {
    auto o = capture_default_options();
    munit_assert_uint(o.version, ==, CAPTURE_API_VERSION);
    munit_assert_size(o.size, ==, sizeof(o));
    munit_assert_int(o.codec, ==, CAPTURE_H265);
    munit_assert_uint(o.bitrate_kbps, ==, 15000);
    munit_assert_uint(o.port, ==, 9295);
    munit_assert_int(capture_validate_options(&o), ==, CAPTURE_OK);
    return MUNIT_OK;
}
static MunitResult validation(const MunitParameter[], void *) {
    auto o = capture_default_options();
    munit_assert_int(capture_validate_options(nullptr), ==, CAPTURE_INVALID_OPTIONS);
    o.version = 99;
    capture_result error = CAPTURE_OK;
    munit_assert_null(capture_engine_create(&o, nullptr, nullptr, &error));
    munit_assert_int(error, ==, CAPTURE_INVALID_OPTIONS);
    o = capture_default_options(); o.port = 1023;
    munit_assert_int(capture_validate_options(&o), ==, CAPTURE_INVALID_OPTIONS);
    o = capture_default_options(); o.bitrate_kbps = 1999;
    munit_assert_int(capture_validate_options(&o), ==, CAPTURE_INVALID_OPTIONS);
    o = capture_default_options(); o.size = 0;
    munit_assert_int(capture_validate_options(&o), ==, CAPTURE_INVALID_OPTIONS);
    for(auto codec : {CAPTURE_H264, CAPTURE_H265, CAPTURE_H265_HDR}) {
        o = capture_default_options(); o.codec = codec;
        munit_assert_int(capture_validate_options(&o), ==, CAPTURE_OK);
    }
    return MUNIT_OK;
}
struct Events {
    std::mutex mutex;
    std::condition_variable changed;
    int idle = 0, stopped = 0;
    std::thread::id thread;
};
static void on_event(capture_state state, void *ctx) {
    auto &events = *static_cast<Events *>(ctx);
    std::lock_guard<std::mutex> lock(events.mutex);
    events.thread = std::this_thread::get_id();
    if(state == CAPTURE_IDLE) ++events.idle;
    if(state == CAPTURE_STOPPED) ++events.stopped;
    events.changed.notify_all();
}
static MunitResult lifecycle(const MunitParameter[], void *) {
    Events events;
    auto options = capture_default_options();
    capture_result error;
    auto engine = capture_engine_create(&options, on_event, &events, &error);
    munit_assert_not_null(engine);
    munit_assert_int(error, ==, CAPTURE_OK);
    // The caller's options can change immediately; the engine must own its copy.
    options.version = 0;
    munit_assert_int(capture_engine_start(engine), ==, CAPTURE_UNAVAILABLE);
    munit_assert_int(capture_engine_stop(engine), ==, CAPTURE_OK);
    munit_assert_int(capture_engine_stop(engine), ==, CAPTURE_OK);
    {
        std::unique_lock<std::mutex> lock(events.mutex);
        bool received = events.changed.wait_for(lock, std::chrono::seconds(3), [&] {
            return events.idle == 1 && events.stopped == 2;
        });
        munit_assert_true(received);
        munit_assert_true(events.thread != std::this_thread::get_id());
    }
    munit_assert_int(capture_engine_destroy(engine), ==, CAPTURE_OK);
    // Joining destroy provides a happens-before boundary for callback ownership.
    munit_assert_int(events.idle, ==, 1);
    munit_assert_int(events.stopped, ==, 2);
    return MUNIT_OK;
}
MunitTest interface_tests[] = {
    {const_cast<char *>("/defaults"), defaults, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
    {const_cast<char *>("/validation"), validation, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
    {const_cast<char *>("/lifecycle"), lifecycle, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
    {nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr}
};
