#include "Timeline.h"
#include <munit.h>
#include <stdexcept>

using namespace capture;
static MunitResult common_clock(const MunitParameter[], void *) {
    Timeline clock({}, 1000000);
    auto video = clock.observe(MediaTrack::Video, 500, 1010000);
    auto audio = clock.observe(MediaTrack::Audio, 2, 1020000, 960);
    munit_assert_false(clock.freeze(1249999));
    munit_assert_false(clock.pts(MediaTrack::Video, video.position).has_value());
    munit_assert_true(clock.freeze(1250000));
    munit_assert_int64(*clock.pts(MediaTrack::Audio, audio.position) - *clock.pts(MediaTrack::Video, video.position), ==, 10000);
    // A reconnect subtracts ONE epoch; it does not re-anchor either track.
    auto v = clock.observe(MediaTrack::Video, 560, 2011000);
    auto a = clock.observe(MediaTrack::Audio, 52, 2021000, 960);
    auto commonEpoch = *clock.pts(MediaTrack::Video, v.position);
    munit_assert_int64(*clock.pts(MediaTrack::Audio, a.position) - commonEpoch, ==, 10000);
    return MUNIT_OK;
}
static MunitResult least_delay_and_offset(const MunitParameter[], void *) {
    Timeline clock({50,1,48000,960,30000}, 0);
    clock.observe(MediaTrack::Video, 10, 50000); clock.observe(MediaTrack::Audio, 10, 40000, 960);
    clock.observe(MediaTrack::Video, 11, 60000); // source=20ms; smaller offset=40ms
    clock.observe(MediaTrack::Audio, 11, 70000, 960); // later jitter must not inflate offset
    munit_assert_true(clock.freeze(250000));
    munit_assert_int64(*clock.pts(MediaTrack::Video, 0), ==, 40000);
    munit_assert_int64(*clock.pts(MediaTrack::Audio, 0), ==, 70000);
    clock.observe(MediaTrack::Video, 12, 65000); // would imply smaller offset after freeze
    munit_assert_int64(*clock.pts(MediaTrack::Video, 0), ==, 40000);
    Timeline negative({50,1,48000,960,-30000}, 0);
    negative.observe(MediaTrack::Video, 0, 10000); negative.observe(MediaTrack::Audio, 0, 10000, 960);
    munit_assert_true(negative.freeze(250000));
    auto epoch = *negative.pts(MediaTrack::Audio, 0);
    munit_assert_int64(epoch, ==, -20000);
    munit_assert_int64(*negative.pts(MediaTrack::Video, 0) - epoch, ==, 30000);
    return MUNIT_OK;
}
static MunitResult wrap_reorder_holes(const MunitParameter[], void *) {
    Timeline clock({}, 0);
    clock.observe(MediaTrack::Video, 65535, 10000); clock.observe(MediaTrack::Audio, 65534, 10000, 960);
    auto v = clock.observe(MediaTrack::Video, 0, 26667);
    auto a = clock.observe(MediaTrack::Audio, 0, 50000, 960, true); // one absent frame, then a hole
    munit_assert_uint64(v.position, ==, 1);
    munit_assert_uint64(a.position, ==, 2);
    munit_assert_true(clock.observe(MediaTrack::Audio, 0, 51000, 960).status == Timeline::Status::Duplicate);
    munit_assert_true(clock.observe(MediaTrack::Video, 65535, 52000).status == Timeline::Status::Reordered);
    munit_assert_true(clock.freeze(250000));
    munit_assert_int64(*clock.pts(MediaTrack::Audio, a.position), ==, 50000);
    munit_assert_int64(*clock.pts(MediaTrack::Video, v.position), ==, 26666);
    return MUNIT_OK;
}
static MunitResult two_hour_rational(const MunitParameter[], void *) {
    Timeline clock({60000,1001,48000,960,0}, 1000000);
    clock.observe(MediaTrack::Video, 65530, 1000000); clock.observe(MediaTrack::Audio, 3, 1000000, 960);
    munit_assert_true(clock.freeze(1250000));
    for(uint64_t i = 1; i <= 432000; ++i) {
        auto videoUs = i * 1000000 * 1001 / 60000;
        auto v = clock.observe(MediaTrack::Video, uint16_t(65530 + i), 1000000 + videoUs);
        munit_assert_true(v.status == Timeline::Status::Accepted);
        munit_assert_uint64(v.position, ==, i);
        munit_assert_int64(*clock.pts(MediaTrack::Video, v.position), ==, videoUs);
        if(i <= 360000) {
            auto a = clock.observe(MediaTrack::Audio, uint16_t(3 + i), 1000000 + i * 20000, 960);
            munit_assert_true(a.status == Timeline::Status::Accepted);
            munit_assert_int64(*clock.pts(MediaTrack::Audio, a.position), ==, i * 20000);
        }
    }
    return MUNIT_OK;
}
static MunitResult validation_and_missing(const MunitParameter[], void *) {
    bool threw = false;
    try { Timeline invalid({0,1,48000,960,0}, 0); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw);
    Timeline missing({}, 0); missing.observe(MediaTrack::Video, 1, 10000);
    missing.observe(MediaTrack::Audio, 1, 20000, 960, true);
    munit_assert_false(missing.freeze(250000)); munit_assert_true(missing.failed());
    Timeline badSamples({}, 0);
    munit_assert_true(badSamples.observe(MediaTrack::Audio, 1, 10000, 480).status == Timeline::Status::Discontinuity);
    Timeline jump({}, 0); jump.observe(MediaTrack::Video, 1, 10000);
    munit_assert_true(jump.observe(MediaTrack::Video, 1000, 20000).status == Timeline::Status::Discontinuity);
    Timeline reset({}, 0); reset.observe(MediaTrack::Video,1000,10000); reset.observe(MediaTrack::Audio,1,20000,960);
    munit_assert_true(reset.freeze(250000));
    // A counter reset with fresh arrivals cannot be ignored forever as old
    // packets. After the no-progress budget it needs an explicit source reset.
    munit_assert_true(reset.observe(MediaTrack::Video,1,2020001).status == Timeline::Status::Discontinuity);
    return MUNIT_OK;
}
int main(int argc, char **argv) {
    MunitTest tests[] = {
        {const_cast<char *>("/common-clock"), common_clock, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/least-delay-offset"), least_delay_and_offset, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/wrap-reorder-holes"), wrap_reorder_holes, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/two-hour-rational"), two_hour_rational, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/validation-missing"), validation_and_missing, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr}};
    MunitSuite suite{const_cast<char *>("/timeline"), tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&suite, nullptr, argc, argv);
}
