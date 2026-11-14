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
static MunitResult counter_rate_diagnostics(const MunitParameter[],void *) {
    // Matching Opus and variable video counters continue arriving without any
    // lost indices. SPS 59.94 timing is not proof of 59.94 transport cadence.
    Timeline clock({60000,1001,48000,480,0},0);
    clock.observe(MediaTrack::Video,0,0);clock.observe(MediaTrack::Audio,0,0,480);
    munit_assert_true(clock.freeze(250000));
    for(uint64_t i=1;i<=18000;++i) {
        munit_assert_true(clock.observe(MediaTrack::Video,uint16_t(i),i*1000000/59).status==Timeline::Status::Accepted);
        munit_assert_true(clock.observe(MediaTrack::Audio,uint16_t(i),i*10000,480).status==Timeline::Status::Accepted);
    }
    auto video=clock.diagnostics(MediaTrack::Video),audio=clock.diagnostics(MediaTrack::Audio);
    munit_assert_uint64(video.position,==,18000);
    munit_assert_uint64(video.elapsedUs,==,18000ull*1000000/59);
    munit_assert_int64(video.driftUs,==,int64_t(18000ull*1000000*1001/60000)-int64_t(video.elapsedUs));
    munit_assert_int64(video.driftUs,<,-4000000);
    munit_assert_uint64(audio.position,==,18000);
    munit_assert_uint64(audio.elapsedUs,==,180000000);
    munit_assert_int64(audio.driftUs,==,0);
    return MUNIT_OK;
}
static MunitResult diagnostics_phase_wrap_and_duplicates(const MunitParameter[],void *) {
    Timeline clock({50,1,48000,480,0},1000000);
    clock.observe(MediaTrack::Video,65530,1050000);clock.observe(MediaTrack::Audio,65530,1040000,480);
    munit_assert_true(clock.freeze(1250000));
    for(uint64_t i=1;i<=100000;++i) {
        clock.observe(MediaTrack::Video,uint16_t(65530+i),1050000+i*20000);
        clock.observe(MediaTrack::Audio,uint16_t(65530+i),1040000+i*10000,480);
    }
    for(auto track:{MediaTrack::Video,MediaTrack::Audio}) {
        auto before=clock.diagnostics(track);
        munit_assert_int64(before.driftUs,==,0);munit_assert_uint64(before.position,==,100000);
        auto samples=track==MediaTrack::Audio?480u:0u;
        auto receipt=track==MediaTrack::Audio?1040000+100000ull*10000:1050000+100000ull*20000;
        clock.observe(track,uint16_t(65530+100000),receipt+1000,samples);
        clock.observe(track,uint16_t(65530+99999),receipt+2000,samples);
        auto after=clock.diagnostics(track);
        munit_assert_uint64(after.elapsedUs,==,before.elapsedUs);
        munit_assert_int64(after.driftUs,==,0);
    }
    return MUNIT_OK;
}
int main(int argc, char **argv) {
    MunitTest tests[] = {
        {const_cast<char *>("/common-clock"), common_clock, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/least-delay-offset"), least_delay_and_offset, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/wrap-reorder-holes"), wrap_reorder_holes, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/two-hour-rational"), two_hour_rational, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/validation-missing"), validation_and_missing, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/counter-rate-diagnostics"), counter_rate_diagnostics, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/diagnostics-phase-wrap-duplicates"), diagnostics_phase_wrap_and_duplicates, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr}};
    MunitSuite suite{const_cast<char *>("/timeline"), tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&suite, nullptr, argc, argv);
}
