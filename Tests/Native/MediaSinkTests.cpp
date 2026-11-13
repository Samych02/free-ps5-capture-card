#include <chiaki/session.h>
#include <chiaki/time.h>
#include <chiaki/fec.h>
#include <munit.h>
#include <vector>
#include <cstring>
#include <thread>
#include <atomic>
#include <memory>
#include <stdexcept>
#include "EncodedMedia.h"

// Small H264 header/slices from the pinned upstream bitstream regression test;
// these exercise the real receiver/frame processor, not a fabricated callback.
static const uint8_t header[] = {
    0,0,0,1,0x67,0x4d,0x40,0x32,0x91,0x8a,1,0xe0,8,0x9f,0x97,1,
    0x6a,2,2,2,0x80,0,3,0xe9,0,1,0xd4,0xc0,0x44,0xd0,0xf1,0xf1,0x50,
    0,0,0,1,0x68,0xee,0x3c,0x80};
static const uint8_t iframe[] = {0,0,0,1,0x65,0x88,0x80,0x82,0x1f,0,0x49,0xee,3,0x29,0xff,0xf8,
    0x7f,0x88,0x46,0x44,0x77,0x17,0xe7,0x6d,0xb3,0xad,0x38,0x19,0x74,0x5a,0xf1,0x51};
static const uint8_t pframe[] = {0,0,0,1,0x41,0x9a,4,0x44,0x3f,0x41,0x5b,0xf4,0x65,0xb4,0x3e,0x1a,
    0xd3,0xa0,0x28,0x1f,0x83,0x63,0x0e,0xc2,0xfc,0x9d,0x7a,0xc7,0xc4,0x7d,0xf9,0x18};
struct Received {
    ChiakiEncodedPacket metadata;
    std::vector<uint8_t> data;
};
struct Fixture {
    ChiakiSession session{};
    ChiakiLog log{};
    ChiakiAudioReceiver audio{};
    ChiakiVideoReceiver video{};
    std::vector<Received> videoPackets, audioPackets;
    std::vector<ChiakiEncodedVideoConfig> videoConfigs;
    std::vector<ChiakiEncodedAudioConfig> audioConfigs;
    std::vector<uint8_t> headerCopy;
    bool accept = true, callbacksUnlocked = true;
    int legacyHeaders = 0, legacyFrames = 0;
    ChiakiEncodedMediaSink sink() {
        return {this,
            [](const ChiakiEncodedVideoConfig *c, void *ctx) {
                auto &f = *static_cast<Fixture *>(ctx);
                f.videoConfigs.push_back(*c); f.headerCopy.assign(c->header, c->header + c->header_size);
            },
            [](const ChiakiEncodedAudioConfig *c, void *ctx) {
                auto &f = *static_cast<Fixture *>(ctx); f.audioConfigs.push_back(*c); f.checkUnlocked();
            },
            [](const ChiakiEncodedPacket *p, void *ctx) {
                auto &f = *static_cast<Fixture *>(ctx);
                munit_assert_false(f.videoConfigs.empty());
                f.videoPackets.push_back({*p, std::vector<uint8_t>(p->data, p->data + p->size)});
                return f.accept;
            },
            [](const ChiakiEncodedPacket *p, void *ctx) {
                auto &f = *static_cast<Fixture *>(ctx); f.checkUnlocked();
                munit_assert_false(f.audioConfigs.empty());
                f.audioPackets.push_back({*p, {}});
                if(p->size) f.audioPackets.back().data.assign(p->data, p->data + p->size);
            }};
    }
    void checkUnlocked() {
        auto result = chiaki_mutex_trylock(&audio.mutex);
        if(result != CHIAKI_ERR_SUCCESS) callbacksUnlocked = false;
        else chiaki_mutex_unlock(&audio.mutex);
    }
    Fixture(bool encoded = true, ChiakiCodec codec = CHIAKI_CODEC_H264) {
        chiaki_log_init(&log, 0, nullptr, nullptr);
        ChiakiConnectInfo info{}; info.host = "127.0.0.1"; info.ps5 = true; info.packet_loss_max = .05;
        info.video_profile = {1920, 1080, 60, 15000, codec};
        munit_assert_int(chiaki_session_init(&session, &info, &log), ==, CHIAKI_ERR_SUCCESS);
        // Corrupt-frame feedback goes only to an invalid descriptor; no network
        // session, console, playback sink, or network thread is started in CI.
        auto &takion = session.stream_connection.takion; takion.sock = -1; takion.log = &log;
        chiaki_mutex_init(&takion.gkcrypt_local_mutex, false);
        chiaki_mutex_init(&takion.seq_num_local_mutex, false);
        munit_assert_int(chiaki_audio_receiver_init(&audio, &session, nullptr), ==, CHIAKI_ERR_SUCCESS);
        chiaki_video_receiver_init(&video, &session, nullptr);
        if(encoded) { auto s = sink(); munit_assert_int(chiaki_session_set_encoded_media_sink(&session, &s), ==, CHIAKI_ERR_SUCCESS); }
    }
    ~Fixture() {
        chiaki_video_receiver_fini(&video); chiaki_audio_receiver_fini(&audio);
        chiaki_mutex_fini(&session.stream_connection.takion.gkcrypt_local_mutex);
        chiaki_mutex_fini(&session.stream_connection.takion.seq_num_local_mutex);
        chiaki_session_fini(&session);
    }
    void audioInfo() {
        ChiakiAudioHeader h{2, 16, 48000, 960, 0}; chiaki_audio_receiver_stream_info(&audio, &h);
    }
    void audioFrame(uint16_t index, bool fec = false) {
        uint8_t bytes[] = {0xf8, 0xff, 0xfe};
        ChiakiTakionAVPacket p{}; p.codec = 5; p.frame_index = fec ? uint16_t(index + 1) : index;
        p.units_in_frame_total = 1; p.units_in_frame_fec = (3 << 8) | (fec ? 0x10 : 1);
        p.data = bytes; p.data_size = sizeof(bytes);
        chiaki_audio_receiver_av_packet(&audio, &p);
        memset(bytes, 0, sizeof(bytes));
    }
    void videoInfo(size_t count = 1, const uint8_t *headers = header, size_t headerSize = sizeof(header)) {
        ChiakiVideoProfile profiles[2]{};
        for(size_t i = 0; i < count; ++i) {
            profiles[i] = {unsigned(1920 / (i + 1)), unsigned(1080 / (i + 1)), headerSize, nullptr};
            profiles[i].header = static_cast<uint8_t *>(malloc(headerSize));
            memcpy(profiles[i].header, headers, headerSize);
        }
        chiaki_video_receiver_stream_info(&video, profiles, count);
    }
    void videoFrame(uint16_t index, bool predicted = false, unsigned profile = 0) {
        videoBytes(index, predicted ? pframe : iframe, sizeof(iframe), profile);
    }
    void videoBytes(uint16_t index, const uint8_t *data, size_t size, unsigned profile = 0) {
        std::vector<uint8_t> bytes(2, 0);
        bytes.insert(bytes.end(), data, data + size);
        ChiakiTakionAVPacket p{}; p.is_video = true; p.frame_index = index; p.units_in_frame_total = 1;
        p.adaptive_stream_index = profile; p.data = bytes.data(); p.data_size = bytes.size();
        chiaki_video_receiver_av_packet(&video, &p);
    }
};
static MunitResult video_delivery(const MunitParameter[], void *) {
    Fixture f; f.videoInfo(2);
    uint64_t before = chiaki_time_now_monotonic_us(); f.videoFrame(1); auto after = chiaki_time_now_monotonic_us();
    f.videoFrame(2, true);
    munit_assert_size(f.videoPackets.size(), ==, 2);
    munit_assert_size(f.videoConfigs.size(), ==, 1);
    munit_assert_uint64(f.videoConfigs[0].generation, ==, 1);
    munit_assert_uint(f.videoConfigs[0].nominal_fps_num, ==, 60);
    munit_assert_memory_equal(sizeof(header), f.headerCopy.data(), header);
    munit_assert_memory_equal(sizeof(iframe), f.videoPackets[0].data.data(), iframe);
    munit_assert_uint(f.videoPackets[0].metadata.frame_index, ==, 1);
    munit_assert_uint64(f.videoPackets[0].metadata.first_receipt_us, >=, before);
    munit_assert_uint64(f.videoPackets[0].metadata.first_receipt_us, <=, after);
    bool tracked = false; for(auto ref : f.video.reference_frames) tracked |= ref == 2;
    munit_assert_true(tracked);
    f.videoFrame(3, false, 1);
    munit_assert_size(f.videoConfigs.size(), ==, 2);
    munit_assert_uint64(f.videoPackets.back().metadata.generation, ==, 2);
    munit_assert_uint(f.videoConfigs.back().width, ==, 960);
    return MUNIT_OK;
}
static MunitResult audio_timing(const MunitParameter[], void *) {
    Fixture f; f.audioInfo();
    auto before = chiaki_time_now_monotonic_us(); f.audioFrame(10); auto after = chiaki_time_now_monotonic_us();
    munit_assert_size(f.audioPackets.size(), ==, 0);
    f.audioFrame(12); f.audioFrame(11); // jitter prefill reorders without restamping
    munit_assert_size(f.audioPackets.size(), ==, 3);
    for(size_t i = 0; i < 3; ++i) {
        munit_assert_uint(f.audioPackets[i].metadata.frame_index, ==, 10 + i);
        munit_assert_uint(f.audioPackets[i].metadata.samples, ==, 960);
        munit_assert_uint64(f.audioPackets[i].metadata.generation, ==, 1);
        munit_assert_uint(f.audioPackets[i].data[0], ==, 0xf8);
    }
    munit_assert_uint64(f.audioPackets[0].metadata.first_receipt_us, >=, before);
    munit_assert_uint64(f.audioPackets[0].metadata.first_receipt_us, <=, after);
    munit_assert_true(f.callbacksUnlocked);
    f.audioFrame(12); munit_assert_size(f.audioPackets.size(), ==, 3);
    f.audioFrame(14); // 13 is missing, explicitly leaves one hole
    munit_assert_size(f.audioPackets.size(), ==, 5);
    munit_assert_uint(f.audioPackets[3].metadata.frame_index, ==, 13);
    munit_assert_uint(f.audioPackets[3].metadata.flags, ==, CHIAKI_ENCODED_MISSING);
    munit_assert_size(f.audioPackets[3].metadata.size, ==, 0);
    f.audioInfo(); f.audioFrame(20); f.audioFrame(21); f.audioFrame(22);
    munit_assert_uint64(f.audioPackets.back().metadata.generation, ==, 2);
    return MUNIT_OK;
}
static MunitResult audio_wrap_fec(const MunitParameter[], void *) {
    Fixture f; f.audioInfo(); f.audioFrame(65534); f.audioFrame(0); f.audioFrame(65535, true);
    munit_assert_size(f.audioPackets.size(), ==, 3);
    munit_assert_uint(f.audioPackets[1].metadata.frame_index, ==, 65535);
    munit_assert_uint(f.audioPackets[1].metadata.flags, ==, CHIAKI_ENCODED_FEC_RECOVERED);
    munit_assert_uint(f.audioPackets[2].metadata.frame_index, ==, 0);
    return MUNIT_OK;
}
static MunitResult video_fec_receipt(const MunitParameter[], void *) {
    Fixture f; f.videoInfo();
    constexpr size_t stride = 32, unitSize = 32;
    uint8_t units[3 * stride]{};
    units[1] = 14; units[stride + 1] = 14; // wire padding to a 32-byte FEC unit
    memcpy(units + 2, iframe, 16); memcpy(units + stride + 2, iframe + 16, 16);
    munit_assert_int(chiaki_fec_encode(units, unitSize, stride, 2, 1), ==, CHIAKI_ERR_SUCCESS);
    ChiakiTakionAVPacket p{}; p.is_video = true; p.frame_index = 1;
    p.units_in_frame_total = 3; p.units_in_frame_fec = 1;
    p.data = units; p.data_size = 18;
    auto before = chiaki_time_now_monotonic_us();
    chiaki_video_receiver_av_packet(&f.video, &p);
    auto after = chiaki_time_now_monotonic_us();
    munit_assert_size(f.videoPackets.size(), ==, 0);
    p.unit_index = 2; p.data = units + 2 * stride; p.data_size = unitSize;
    chiaki_video_receiver_av_packet(&f.video, &p);
    munit_assert_size(f.videoPackets.size(), ==, 1);
    munit_assert_uint(f.videoPackets[0].metadata.flags, ==, CHIAKI_ENCODED_FEC_RECOVERED);
    munit_assert_uint64(f.videoPackets[0].metadata.first_receipt_us, >=, before);
    munit_assert_uint64(f.videoPackets[0].metadata.first_receipt_us, <=, after);
    munit_assert_memory_equal(sizeof(iframe), f.videoPackets[0].data.data(), iframe);
    return MUNIT_OK;
}
static MunitResult video_wrap_loss(const MunitParameter[], void *) {
    Fixture f; f.videoInfo(); f.video.frame_index_prev_complete = 65534;
    f.video.frames_lost = 2;
    f.videoFrame(65535); f.videoFrame(0, true);
    munit_assert_size(f.videoPackets.size(), ==, 2);
    munit_assert_uint(f.videoPackets[0].metadata.frame_index, ==, 65535);
    munit_assert_uint(f.videoPackets[1].metadata.frame_index, ==, 0);
    munit_assert_uint(f.videoPackets[0].metadata.lost_frames, ==, 2);
    munit_assert_uint(f.videoPackets[1].metadata.lost_frames, ==, 0);
    return MUNIT_OK;
}
static MunitResult video_loss_across_wrap(const MunitParameter[], void *) {
    Fixture f; f.videoInfo(); f.video.frame_index_prev_complete = 65534;
    uint8_t bytes[32]{};
    ChiakiTakionAVPacket p{}; p.is_video = true; p.frame_index = 0;
    p.units_in_frame_total = 3; p.units_in_frame_fec = 1; p.unit_index = 2;
    p.data = bytes; p.data_size = sizeof(bytes);
    // Only a parity unit: two source units are absent, so FEC cannot recover.
    chiaki_video_receiver_av_packet(&f.video, &p);
    munit_assert_size(f.videoPackets.size(), ==, 0);
    munit_assert_int(f.video.frames_lost, ==, 2); // frames 65535 and 0
    f.videoFrame(1);
    munit_assert_size(f.videoPackets.size(), ==, 1);
    munit_assert_uint(f.videoPackets[0].metadata.lost_frames, ==, 2);
    return MUNIT_OK;
}
static MunitResult idr_lifetime(const MunitParameter[], void *) {
    Fixture f(false);
    // No transport has been started: it is not safe to touch Takion's internals.
    munit_assert_int(chiaki_session_request_idr(&f.session), ==, CHIAKI_ERR_UNINITIALIZED);
    // Simulate an open transport using only an invalid socket descriptor. Stop
    // must close the send gate before Takion's mutexes could be finalized.
    chiaki_mutex_lock(&f.session.stream_connection.idr_mutex);
    f.session.stream_connection.idr_transport_active = true;
    chiaki_mutex_unlock(&f.session.stream_connection.idr_mutex);
    munit_assert_int(chiaki_session_request_idr(&f.session), ==, CHIAKI_ERR_NETWORK);
    std::atomic<bool> go{false}; std::atomic<int> unexpected{0};
    std::thread requests([&] {
        while(!go.load()) std::this_thread::yield();
        for(int i = 0; i < 1000; ++i)
        {
            auto result = chiaki_session_request_idr(&f.session);
            if(result != CHIAKI_ERR_UNINITIALIZED && result != CHIAKI_ERR_NETWORK) ++unexpected;
        }
    });
    go = true; chiaki_session_stop(&f.session); requests.join();
    munit_assert_int(unexpected.load(), ==, 0);
    munit_assert_int(chiaki_session_request_idr(&f.session), ==, CHIAKI_ERR_UNINITIALIZED);
    return MUNIT_OK;
}
static MunitResult sink_rules(const MunitParameter[], void *) {
    Fixture f(false); auto sink = f.sink(); auto invalid = sink; invalid.audio_packet = nullptr;
    munit_assert_int(chiaki_session_set_encoded_media_sink(&f.session, &invalid), ==, CHIAKI_ERR_INVALID_DATA);
    chiaki_session_set_video_sample_cb(&f.session, [](uint8_t *, size_t, int32_t, bool, void *) { return true; }, nullptr);
    munit_assert_int(chiaki_session_set_encoded_media_sink(&f.session, &sink), ==, CHIAKI_ERR_INVALID_DATA);
    chiaki_session_set_video_sample_cb(&f.session, nullptr, nullptr);
    munit_assert_int(chiaki_session_set_encoded_media_sink(&f.session, &sink), ==, CHIAKI_ERR_SUCCESS);
    ChiakiAudioSink legacy{nullptr, nullptr, [](uint8_t *, size_t, void *) {}};
    chiaki_session_set_audio_sink(&f.session, &legacy);
    munit_assert_int(chiaki_session_start(&f.session), ==, CHIAKI_ERR_INVALID_DATA);
    f.session.audio_sink = {}; f.session.media_started = true;
    munit_assert_int(chiaki_session_set_encoded_media_sink(&f.session, &sink), ==, CHIAKI_ERR_INVALID_DATA);
    return MUNIT_OK;
}
static MunitResult acceptance(const MunitParameter[], void *) {
    Fixture f; f.videoInfo(); f.accept = false; f.videoFrame(1);
    bool tracked = false; for(auto ref : f.video.reference_frames) tracked |= ref == 1;
    munit_assert_false(tracked);
    f.accept = true; f.videoFrame(2, true); // cannot reference a rejected frame
    munit_assert_size(f.videoPackets.size(), ==, 1);
    return MUNIT_OK;
}
static MunitResult legacy_delivery(const MunitParameter[], void *) {
    Fixture f(false);
    ChiakiAudioSink sink{&f,
        [](ChiakiAudioHeader *, void *p) { ++static_cast<Fixture *>(p)->legacyHeaders; },
        [](uint8_t *, size_t, void *p) { ++static_cast<Fixture *>(p)->legacyFrames; }};
    chiaki_session_set_audio_sink(&f.session, &sink);
    chiaki_session_set_video_sample_cb(&f.session, [](uint8_t *, size_t, int32_t, bool, void *p) {
        ++static_cast<Fixture *>(p)->legacyFrames; return true;
    }, &f);
    f.audioInfo(); f.audioFrame(1); f.audioFrame(2); f.audioFrame(3);
    f.videoInfo(); f.videoFrame(1); f.videoFrame(2, true);
    munit_assert_int(f.legacyHeaders, ==, 1);
    munit_assert_int(f.legacyFrames, ==, 6); // 3 Opus + video header + I + P
    return MUNIT_OK;
}
static MunitResult owned_adapter(const MunitParameter[], void *) {
    Fixture f(false);
    std::vector<capture::EncodedEvent> events;
    capture::EncodedMedia adapter([&](capture::EncodedEvent e) { events.push_back(std::move(e)); return true; });
    auto sink = adapter.sink();
    munit_assert_int(chiaki_session_set_encoded_media_sink(&f.session, &sink), ==, CHIAKI_ERR_SUCCESS);
    f.videoInfo(); f.videoFrame(1); f.audioInfo(); f.audioFrame(1); f.audioFrame(2); f.audioFrame(3);
    munit_assert_size(events.size(), ==, 6);
    munit_assert_null(events[0].video.header);
    munit_assert_null(events[1].packet.data);
    munit_assert_memory_equal(sizeof(header), events[0].bytes.data(), header);
    munit_assert_memory_equal(sizeof(iframe), events[1].bytes.data(), iframe);
    munit_assert_false(adapter.failed());
    return MUNIT_OK;
}
static MunitResult adapter_bounds(const MunitParameter[], void *) {
    capture::EncodedMedia adapter;
    auto sink = adapter.sink();
    munit_assert_true(sink.video_packet != nullptr);
    ChiakiEncodedPacket packet{};
    // The claimed size must be rejected before dereferencing/copying this byte.
    uint8_t byte = 0; packet.data = &byte; packet.size = capture::EncodedMedia::MaxPacket + 1;
    munit_assert_false(sink.video_packet(&packet, sink.user));
    munit_assert_true(adapter.failed());
    capture::EncodedMedia noConsumer; sink = noConsumer.sink();
    packet.size = 1;
    munit_assert_true(sink.video_packet(&packet, sink.user));
    capture::EncodedMedia largeHeader; sink = largeHeader.sink();
    ChiakiEncodedVideoConfig config{}; config.header = &byte; config.header_size = capture::EncodedMedia::MaxHeader + 1;
    sink.video_config(&config, sink.user); munit_assert_true(largeHeader.failed());
    capture::EncodedMedia throwing([](capture::EncodedEvent) -> bool { throw std::runtime_error("test consumer"); });
    sink = throwing.sink();
    munit_assert_false(sink.video_packet(&packet, sink.user));
    munit_assert_true(throwing.failed());
    return MUNIT_OK;
}
static MunitResult control_lifetime(const MunitParameter[], void *) {
    auto f = std::make_unique<Fixture>(false); capture::SessionMediaControl control(&f->session);
    munit_assert_int(control.requestKeyframe(), ==, CHIAKI_ERR_UNINITIALIZED);
    std::atomic<bool> go{false}; std::atomic<int> unexpected{0};
    std::thread thread([&] {
        while(!go.load()) std::this_thread::yield();
        for(int i = 0; i < 1000; ++i) {
            auto result = control.requestKeyframe();
            if(result != CHIAKI_ERR_UNINITIALIZED && result != CHIAKI_ERR_CANCELED) ++unexpected;
        }
    });
    go = true;
    munit_assert_ptr_equal(control.stopAndDetach(), &f->session);
    munit_assert_true(f->session.should_stop);
    f.reset(); // request thread may still run, but can no longer touch a session
    thread.join();
    munit_assert_null(control.stopAndDetach());
    munit_assert_int(control.requestKeyframe(), ==, CHIAKI_ERR_CANCELED);
    munit_assert_int(unexpected.load(), ==, 0);
    return MUNIT_OK;
}
static MunitResult hevc_configuration(const MunitParameter[], void *) {
    // From pinned upstream bitstream.c. This verifies raw HEVC metadata/payload
    // delivery for both HEVC mode values, NOT that this SDR fixture is HDR.
    const uint8_t hevcHeader[] = {
        0,0,0,1,0x40,1,0x0c,1,0xff,0xff,1,0x60,0,0,3,0,
        0xb0,0,0,3,0,0,3,0,0x96,0x0a,0xc0,0x90,0,0,0,1,
        0x42,1,1,1,0x60,0,0,3,0,0xb0,0,0,3,0,0,3,
        0,0x96,0xa0,3,0xc0,0x80,0x11,7,0xcb,0xc2,0xb9,0x24,0x29,0x52,0x70,0x16,
        0xa0,0x20,0x20,0x20,0x80,0,7,0xd2,0,1,0xd4,0xc0,0x20,0xe5,0xa1,0xe3,
        0xd0,0,0,0,1,0x44,1,0xc0,0xf3,0xc0,0x4c,0x90};
    const uint8_t hevcFrame[] = {0,0,0,1,0x28,1,0xac,0x25,0xcf,0x83,0xff,0x23,0x54,0xab,0x5c,0xf5,
        0x7a,6,0x7c,0x3f,0x31,0x9b,0xe6,0x10,0x57,0xe8,0x0e,0xcf,0xdd,0xda,0xdb,0x3f};
    const uint8_t hevcPredicted[] = {0,0,0,1,2,1,0xd7,0x85,0x6a,0xae,0xa6,0x11,0x80,0x95,0x80,0x0a,
        0xec,0x5e,0xdf,0x39,0x86,0xe6,0xd9,7,0x49,0x17,0xe2,0x62,0x57,0x14,0xd7,8};
    for(auto codec : {CHIAKI_CODEC_H265, CHIAKI_CODEC_H265_HDR}) {
        Fixture f(true, codec); f.videoInfo(1, hevcHeader, sizeof(hevcHeader)); f.videoBytes(1, hevcFrame, sizeof(hevcFrame));
        munit_assert_size(f.videoConfigs.size(), ==, 1);
        munit_assert_int(f.videoConfigs[0].codec, ==, codec);
        munit_assert_memory_equal(sizeof(hevcHeader), f.headerCopy.data(), hevcHeader);
        munit_assert_size(f.videoPackets.size(), ==, 1);
        munit_assert_memory_equal(sizeof(hevcFrame), f.videoPackets[0].data.data(), hevcFrame);
        // The upstream ref-5 fixture advertises nine negative pictures. Frame
        // 9's referenced frame 3 is missing, but accepted frame 1 is available
        // at ref-7. A single-reference P fixture cannot represent this recovery.
        f.videoBytes(9, hevcPredicted, sizeof(hevcPredicted));
        munit_assert_size(f.videoPackets.size(), ==, 2);
        munit_assert_uint(f.videoPackets[1].metadata.flags, ==, CHIAKI_ENCODED_REFERENCE_REWRITTEN);
    }
    return MUNIT_OK;
}
static MunitResult core_allocation_bounds(const MunitParameter[], void *) {
    Fixture f; f.videoInfo();
    uint8_t prefix[]{0,0}; ChiakiTakionAVPacket packet{};
    packet.is_video=true; packet.frame_index=1; packet.units_in_frame_total=511;
    packet.data=prefix; packet.data_size=8193;
    munit_assert_int(chiaki_frame_processor_alloc_frame(&f.video.frame_processor,&packet), ==, CHIAKI_ERR_INVALID_DATA);
    munit_assert_size(f.video.frame_processor.frame_buf_size, <=, 4*1024*1024);
    chiaki_video_receiver_av_packet(&f.video,&packet);
    chiaki_video_receiver_av_packet(&f.video,&packet);
    munit_assert_size(f.videoPackets.size(), ==, 0);
    return MUNIT_OK;
}
int main(int argc, char **argv) {
    munit_assert_int(chiaki_lib_init(), ==, CHIAKI_ERR_SUCCESS);
    MunitTest tests[] = {
        {const_cast<char *>("/video-delivery"), video_delivery, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/audio-timing"), audio_timing, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/audio-wrap-fec"), audio_wrap_fec, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/video-fec-receipt"), video_fec_receipt, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/video-wrap-loss"), video_wrap_loss, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/video-loss-across-wrap"), video_loss_across_wrap, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/idr-lifetime"), idr_lifetime, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/sink-rules"), sink_rules, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/acceptance"), acceptance, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/legacy"), legacy_delivery, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/owned-adapter"), owned_adapter, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/adapter-bounds"), adapter_bounds, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/control-lifetime"), control_lifetime, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/hevc-configuration"), hevc_configuration, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {const_cast<char *>("/core-allocation-bounds"), core_allocation_bounds, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr},
        {nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr}};
    MunitSuite suite{const_cast<char *>("/media-sink"), tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&suite, nullptr, argc, argv);
}
