#include "AnnexB.h"
#include <vector>
#include "AudioFormat.h"
#include <munit.h>
#include <stdexcept>
#include <cstring>
using namespace capture;
static MunitResult access_h264(const MunitParameter[], void *) {
    const uint8_t idr[]{0,0,0,1,0x67,1,0,0,1,0x65,0x88};
    auto info = inspectAccessUnit(CHIAKI_CODEC_H264,idr,sizeof(idr));
    munit_assert_true(info.hasPicture); munit_assert_true(info.access == VideoAccess::IDR);
    const uint8_t notIdr[]{0,0,1,0x41,0x88}; // I-slice bits do not turn NAL type 1 into IDR
    info = inspectAccessUnit(CHIAKI_CODEC_H264,notIdr,sizeof(notIdr));
    munit_assert_true(info.hasPicture); munit_assert_true(info.access == VideoAccess::Predicted);
    const uint8_t headers[]{0,0,1,0x67,1,0,0,1,0x68,1};
    munit_assert_false(inspectAccessUnit(CHIAKI_CODEC_H264,headers,sizeof(headers)).hasPicture);
    auto signature=parameterSetSignature(CHIAKI_CODEC_H264,headers,sizeof(headers));
    munit_assert_uint64(signature, !=, 0);
    std::vector<uint8_t> repeated(headers,headers+sizeof(headers));
    repeated.insert(repeated.end(),headers,headers+sizeof(headers));
    munit_assert_uint64(parameterSetSignature(CHIAKI_CODEC_H264,repeated.data(),repeated.size()), ==, signature);
    munit_assert_uint64(parameterSetSignature(CHIAKI_CODEC_H264,notIdr,sizeof(notIdr)), ==, 0);
    return MUNIT_OK;
}
static MunitResult access_hevc(const MunitParameter[], void *) {
    for(auto codec : {CHIAKI_CODEC_H265, CHIAKI_CODEC_H265_HDR}) {
        for(uint8_t type : {16,17,18,19,20,21,6,7,8,9,1}) {
            uint8_t data[]{0,0,0,1,uint8_t(type<<1),1,0x80};
            auto info = inspectAccessUnit(codec,data,sizeof(data));
            munit_assert_true(info.hasPicture);
            auto expected = type == 21 ? VideoAccess::CRA : type >= 16 ? VideoAccess::IDR :
                type >= 6 && type <= 9 ? VideoAccess::Leading : VideoAccess::Predicted;
            munit_assert_true(info.access == expected);
        }
    }
    return MUNIT_OK;
}
static MunitResult malformed_units(const MunitParameter[], void *) {
    const uint8_t missing[]{0,0,0,2,0x65};
    const uint8_t truncated[]{0,0,1,0x26};
    const uint8_t forbidden[]{0,0,1,0xe5,1};
    const uint8_t mixed[]{0,0,1,0x65,1,0,0,1,0x41,1};
    bool threw = false;
    try { inspectAccessUnit(CHIAKI_CODEC_H264,missing,sizeof(missing)); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw); threw = false;
    try { inspectAccessUnit(CHIAKI_CODEC_H265,truncated,sizeof(truncated)); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw); threw = false;
    try { inspectAccessUnit(CHIAKI_CODEC_H264,forbidden,sizeof(forbidden)); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw); threw = false;
    try { inspectAccessUnit(CHIAKI_CODEC_H264,mixed,sizeof(mixed)); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw); threw = false;
    try { inspectAccessUnit(CHIAKI_CODEC_H264,missing,PacketQueue::MaxPacket+1); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw);
    return MUNIT_OK;
}
static MunitResult live_opus_head(const MunitParameter[], void *) {
    auto header = liveOpusHead({1,2,48000,960});
    munit_assert_memory_equal(8,header.data(),"OpusHead");
    munit_assert_uint(header[8], ==, 1); munit_assert_uint(header[9], ==, 2);
    munit_assert_uint(header[10] | header[11], ==, 0); // no encoder fixture's pre-skip
    munit_assert_uint(header[12] | (header[13]<<8) | (header[14]<<16) | (header[15]<<24), ==, 48000);
    munit_assert_uint(header[16] | header[17] | header[18], ==, 0);
    bool threw = false;
    try { liveOpusHead({1,6,48000,960}); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw); threw = false;
    try { liveOpusHead({1,2,44100,960}); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw);
    return MUNIT_OK;
}
static MunitResult opus_durations(const MunitParameter[], void *) {
    const uint8_t twenty[]{0xf8,0xff,0xfe};
    const uint8_t five[]{0x88,0};
    const uint8_t sixty[]{0x18,0};
    const uint8_t multi[]{0x83,4,0}; // four 2.5ms frames
    munit_assert_uint(opusPacketSamples(twenty,sizeof(twenty)), ==, 960);
    munit_assert_uint(opusPacketSamples(five,sizeof(five)), ==, 240);
    munit_assert_uint(opusPacketSamples(sixty,sizeof(sixty)), ==, 2880);
    munit_assert_uint(opusPacketSamples(multi,sizeof(multi)), ==, 480);
    const uint8_t invalid[]{0xfb,63}; bool threw = false;
    try { opusPacketSamples(invalid,sizeof(invalid)); } catch(const std::invalid_argument &) { threw = true; }
    munit_assert_true(threw);
    return MUNIT_OK;
}
int main(int argc,char **argv) {
    MunitTest tests[] = {
        {const_cast<char *>("/h264-access"),access_h264,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/hevc-access"),access_hevc,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/malformed"),malformed_units,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/live-opus-head"),live_opus_head,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/opus-duration"),opus_durations,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {nullptr,nullptr,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr}};
    MunitSuite suite{const_cast<char *>("/codec-units"),tests,nullptr,1,MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&suite,nullptr,argc,argv);
}
