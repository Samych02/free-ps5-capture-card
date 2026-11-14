#include "Diagnostics.h"
#include "Pipeline.h"
#include <chiaki/time.h>
#include <munit.h>
#include <cstring>
extern "C" {
#include <libavutil/error.h>
}
static MunitResult quit_policy(const MunitParameter[],void *) {
    auto transient=capture::classifyQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED);
    munit_assert_true(transient.done);munit_assert_true(transient.retry);
    auto auth=capture::classifyQuit(CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED);
    munit_assert_false(auth.retry);munit_assert_int(auth.status.error,==,CAPTURE_ERROR_IDENTITY);
    for(auto code:{CHIAKI_RP_APPLICATION_REASON_REGIST_FAILED,CHIAKI_RP_APPLICATION_REASON_INVALID_PSN_ID}) {
        auto rejected=capture::classifyQuit(chiaki_session_reason_from_application_reason(code));
        munit_assert_false(rejected.retry);munit_assert_int(rejected.status.error,==,CAPTURE_ERROR_IDENTITY);
    }
    auto busy=capture::classifyQuit(CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE);
    munit_assert_false(busy.retry);munit_assert_int(busy.status.error,==,CAPTURE_ERROR_IN_USE);
    munit_assert_true(strlen(busy.status.message)>0);
    auto stop=capture::classifyQuit(CHIAKI_QUIT_REASON_STOPPED);
    munit_assert_false(stop.retry);munit_assert_int(stop.status.state,==,CAPTURE_STOPPED);
    for(unsigned i=1;i<=8;++i) {
        unsigned delay=capture::retryDelayMilliseconds(i);
        const unsigned expected[]{1000,2000,4000,8000,15000,15000,15000,15000};
        munit_assert_uint(delay,>=,expected[i-1]);munit_assert_uint(delay,<,expected[i-1]+200);
    }
    munit_assert_uint(capture::nextRetryAttempt(4,30000000),==,1);
    munit_assert_uint(capture::nextRetryAttempt(4,29999999),==,5);
    return MUNIT_OK;
}
static MunitResult loss_counters(const MunitParameter[],void *) {
    capture::LocalTCPServer server(0);
    capture_options options{sizeof(capture_options),CAPTURE_API_VERSION,1920,1080,60,15000,9295,CAPTURE_H265};
    capture::MediaPipeline pipeline(options,server,[]{});
    auto sink=pipeline.sink();
    ChiakiEncodedAudioConfig audio{1,2,48000,960};sink.audio_config(&audio,sink.user);
    ChiakiEncodedPacket hole{1,chiaki_time_now_monotonic_us(),0,CHIAKI_ENCODED_MISSING,960,0,nullptr,0};
    sink.audio_packet(&hole,sink.user);
    auto status=pipeline.snapshot().status;
    munit_assert_uint64(status.missing_audio_packets,==,1);
    munit_assert_uint64(status.audio_packets,==,0);
    munit_assert_uint64(status.video_packets,==,0);
    munit_assert_uint64(status.queue_age_us,==,0);
    pipeline.stopAndJoin();return MUNIT_OK;
}
static MunitResult output_reset_reasons(const MunitParameter[],void *) {
    using Reason=capture::PacketQueue::Reason;
    munit_assert_not_null(strstr(capture::outputQueueResetMessage(Reason::StaleMedia),"stale"));
    munit_assert_not_null(strstr(capture::outputQueueResetMessage(Reason::MissingTrack),"track"));
    munit_assert_not_null(strstr(capture::outputQueueResetMessage(Reason::Overflow),"storage"));
    munit_assert_not_null(strstr(capture::outputQueueResetMessage(Reason::KeyframeTimeout),"keyframe"));
    munit_assert_not_null(strstr(capture::outputWriteResetMessage(AVERROR(ETIMEDOUT)),"stalled"));
    munit_assert_not_null(strstr(capture::outputWriteResetMessage(AVERROR(EPIPE)),"closed"));
    munit_assert_not_null(strstr(capture::outputWriteResetMessage(AVERROR_EXIT),"cancelled"));
    return MUNIT_OK;
}
static MunitResult missing_track_messages(const MunitParameter[],void *) {
    using Queue=capture::PacketQueue;
    Queue::Stats state{}; state.reason=Queue::Reason::MissingTrack;
    state.missing.audioAgeUs=251999;state.missing.videoAgeUs=1000;
    for(auto stage:{Queue::MissingStage::Configuration,Queue::MissingStage::StartupAlignment,
                    Queue::MissingStage::Audio,Queue::MissingStage::Video,Queue::MissingStage::Both}) {
        state.missing.stage=stage;
        state.missing.audioStartDeltaUs=-301000;
        auto message=capture::outputQueueResetMessage(state);
        const char *expected=stage==Queue::MissingStage::Configuration?"configuration":
            stage==Queue::MissingStage::StartupAlignment?"startup alignment":
            stage==Queue::MissingStage::Audio?"audio delivery":
            stage==Queue::MissingStage::Video?"video delivery":"both tracks";
        munit_assert_not_null(strstr(message.c_str(),expected));
        munit_assert_not_null(strstr(message.c_str(),"audio age 251 ms"));
        munit_assert_not_null(strstr(message.c_str(),"video age 1 ms"));
        if(stage==Queue::MissingStage::StartupAlignment)
            munit_assert_not_null(strstr(message.c_str(),"audio-start -301 ms"));
        munit_assert_size(message.size(),<,sizeof(capture_status{}.message));
    }
    return MUNIT_OK;
}
int main(int argc,char **argv) {
    MunitTest tests[]{{const_cast<char *>("/quit-policy"),quit_policy,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/loss-counters"),loss_counters,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/output-reset-reasons"),output_reset_reasons,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/missing-track-messages"),missing_track_messages,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {nullptr,nullptr,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr}};
    MunitSuite suite{const_cast<char *>("/diagnostics"),tests,nullptr,1,MUNIT_SUITE_OPTION_NONE};return munit_suite_main(&suite,nullptr,argc,argv);
}
