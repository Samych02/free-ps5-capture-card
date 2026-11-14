#include "Diagnostics.h"
#include "Pipeline.h"
#include <chiaki/time.h>
#include <munit.h>
#include <cstring>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <atomic>
#include <stdexcept>
extern "C" {
#include <libavutil/error.h>
#include <libavutil/mastering_display_metadata.h>
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
static MunitResult owned_release_grace_is_bounded(const MunitParameter[],void *) {
    using Grace=capture::SessionReleaseGrace;
    for(uint64_t start:{0ull,1000000ull}) {
        Grace grace;
        munit_assert_false(grace.allowsBusyRetry(start));
        grace.toreDown(false,true,start);
        munit_assert_false(grace.allowsBusyRetry(start));
        grace.toreDown(true,false,start);
        munit_assert_false(grace.allowsBusyRetry(start));
        grace.toreDown(true,true,start);
        munit_assert_true(grace.allowsBusyRetry(start));
        munit_assert_true(grace.allowsBusyRetry(start+Grace::WindowUs-1));
        if(start) munit_assert_false(grace.allowsBusyRetry(start-1));
        grace.toreDown(false,true,start+Grace::WindowUs-1);
        munit_assert_false(grace.allowsBusyRetry(start+Grace::WindowUs));
        grace.established();
        munit_assert_false(grace.allowsBusyRetry(start));
    }
    return MUNIT_OK;
}
static MunitResult dynamic_hdr_metadata(const MunitParameter[],void *) {
    const uint8_t original[]{0,0,0,1,0x4e,1,144,4,3,0xe8,1,0x90,0x80};
    const uint8_t changed[]{0,0,0,1,0x4e,1,144,4,7,0xd0,1,0x90,0x80};
    auto state=capture::readHDRMetadata(original,sizeof(original));
    auto update=capture::readHDRMetadata(changed,sizeof(changed));
    munit_assert_not_null(state.get());munit_assert_not_null(update.get());
    munit_assert_true(capture::applyHDRMetadata(state.get(),update.get()));
    auto side=av_packet_side_data_get(state->coded_side_data,state->nb_coded_side_data,AV_PKT_DATA_CONTENT_LIGHT_LEVEL);
    munit_assert_not_null(side);
    munit_assert_uint(reinterpret_cast<const AVContentLightMetadata *>(side->data)->MaxCLL,==,2000);
    munit_assert_false(capture::applyHDRMetadata(state.get(),update.get()));
    const uint8_t malformed[]{0,0,0,1,0x4e,1,144,4,7,0x80};bool rejected=false;
    try {capture::readHDRMetadata(malformed,sizeof(malformed));}catch(...) {rejected=true;}
    munit_assert_true(rejected);
    return MUNIT_OK;
}
static MunitResult partial_parameter_updates(const MunitParameter[],void *) {
    const uint8_t base[]{0,0,0,1,0x67,66,0,30,0x80,0,0,0,1,0x68,0xc0};
    const uint8_t sameSPS[]{0,0,1,0x67,66,0,30,0x80};
    munit_assert_false(capture::updatedVideoHeader(CHIAKI_CODEC_H264,base,sizeof(base),sameSPS,sizeof(sameSPS)).has_value());
    const uint8_t changedSPS[]{0,0,1,0x67,66,0,31,0x80};
    auto changed=capture::updatedVideoHeader(CHIAKI_CODEC_H264,base,sizeof(base),changedSPS,sizeof(changedSPS));
    munit_assert_true(changed.has_value());
    const uint8_t expected[]{0,0,0,1,0x67,66,0,31,0x80,0,0,0,1,0x68,0xc0};
    munit_assert_size(changed->size(),==,sizeof(expected));
    munit_assert_memory_equal(sizeof(expected),changed->data(),expected);
    const uint8_t extraPPS[]{0,0,1,0x68,0x50};
    auto extended=capture::updatedVideoHeader(CHIAKI_CODEC_H264,base,sizeof(base),extraPPS,sizeof(extraPPS));
    munit_assert_true(extended.has_value());
    munit_assert_size(extended->size(),==,sizeof(base)+6);
    munit_assert_memory_equal(sizeof(base),extended->data(),base);
    return MUNIT_OK;
}
static MunitResult stale_parser_cannot_fail_new_generation(const MunitParameter[],void *) {
    std::mutex gate;std::condition_variable changed;bool entered=false,release=false;
    std::atomic<unsigned> parses{0};std::atomic<bool> wrongHeader{false};
    capture::LocalTCPServer server(0);
    capture_options options{sizeof(capture_options),CAPTURE_API_VERSION,1920,1080,60,15000,9295,CAPTURE_H264};
    capture::MediaPipeline pipeline(options,server,[]{},[&](ChiakiCodec,const uint8_t *header,size_t size,const uint8_t *,size_t) {
        if(++parses==1) {
            std::unique_lock<std::mutex> lock(gate);entered=true;changed.notify_all();
            changed.wait(lock,[&]{return release;});throw std::runtime_error("old parser rejected obsolete metadata");
        }
        wrongHeader=size<8 || header[7]!=31;
        capture::VideoFormat format;format.parameters.reset(avcodec_parameters_alloc());
        format.parameters->width=640;format.parameters->height=360;format.frameRate={60,1};format.depth=8;
        return format;
    });
    auto sink=pipeline.sink();
    const uint8_t header[]{0,0,0,1,0x67,66,0,30,0x80,0,0,0,1,0x68,0xc0};
    ChiakiEncodedVideoConfig video{1,CHIAKI_CODEC_H264,640,360,60,1,header,sizeof(header)};
    ChiakiEncodedAudioConfig audio{1,2,48000,960};sink.video_config(&video,sink.user);sink.audio_config(&audio,sink.user);
    const uint8_t opus[]{0xf8,0},picture[]{0,0,0,1,0x65,0x80};
    uint16_t audioIndex=0,videoIndex=0;
    auto feed=[&](uint64_t generation) {
        auto now=chiaki_time_now_monotonic_us();
        ChiakiEncodedPacket a{1,now,audioIndex++,0,960,0,opus,sizeof(opus)},v{generation,now,videoIndex++,0,0,0,picture,sizeof(picture)};
        sink.audio_packet(&a,sink.user);sink.video_packet(&v,sink.user);
    };
    feed(1);
    {std::unique_lock<std::mutex> lock(gate);munit_assert_true(changed.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
    uint8_t newer[sizeof(header)];memcpy(newer,header,sizeof(header));newer[7]=31;
    video.generation=2;video.header=newer;sink.video_config(&video,sink.user);feed(2);
    {std::lock_guard<std::mutex> lock(gate);release=true;changed.notify_all();}
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(pipeline.snapshot().status.width!=640 && std::chrono::steady_clock::now()<deadline) {
        feed(2);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto result=pipeline.snapshot();
    munit_assert_false(result.done);munit_assert_false(result.retry);munit_assert_uint(result.status.width,==,640);
    munit_assert_uint(parses.load(),==,2);munit_assert_false(wrongHeader.load());
    pipeline.stopAndJoin();return MUNIT_OK;
}
int main(int argc,char **argv) {
    MunitTest tests[]{{const_cast<char *>("/quit-policy"),quit_policy,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/loss-counters"),loss_counters,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/output-reset-reasons"),output_reset_reasons,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/missing-track-messages"),missing_track_messages,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/owned-release-grace"),owned_release_grace_is_bounded,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/dynamic-hdr-metadata"),dynamic_hdr_metadata,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/partial-parameter-updates"),partial_parameter_updates,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/stale-parser-generation"),stale_parser_cannot_fail_new_generation,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {nullptr,nullptr,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr}};
    MunitSuite suite{const_cast<char *>("/diagnostics"),tests,nullptr,1,MUNIT_SUITE_OPTION_NONE};return munit_suite_main(&suite,nullptr,argc,argv);
}
