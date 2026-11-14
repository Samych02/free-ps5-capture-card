#include "PacketQueue.h"
#include <munit.h>
#include <thread>
#include <vector>
#include <cstring>
using namespace capture;
struct Fixture {
    PacketQueue queue;
    explicit Fixture(size_t limit = PacketQueue::MaxBytes) : queue(limit) {
        uint8_t header[]{0,0,0,1,0x67};
        ChiakiEncodedVideoConfig video{1, CHIAKI_CODEC_H264, 1920,1080,60,1,header,sizeof(header)};
        ChiakiEncodedAudioConfig audio{1,2,48000,960};
        munit_assert_true(queue.configureVideo(video) == PacketQueue::Result::Accepted);
        munit_assert_true(queue.configureAudio(audio) == PacketQueue::Result::Accepted);
        memset(header, 0, sizeof(header));
    }
    PacketQueue::Result push(MediaTrack track, int64_t pts, VideoAccess access = VideoAccess::Predicted,
                             uint64_t generation = 1, size_t size = 3, uint64_t now = 100000,
                             uint32_t flags = 0) {
        std::vector<uint8_t> bytes(size, 9);
        ChiakiEncodedPacket packet{generation,now,uint16_t(pts / 10000),flags,track == MediaTrack::Audio ? 960u : 0u,0,bytes.data(),size};
        auto result = queue.push(track, packet, {pts,pts}, access, now);
        std::fill(bytes.begin(), bytes.end(), 0);
        return result;
    }
};
static MunitResult startup_and_owned(const MunitParameter[], void *) {
    Fixture f;
    auto initial = f.queue.stats().bytes;
    f.push(MediaTrack::Video, 0, VideoAccess::IDR);
    munit_assert_size(f.queue.stats().bytes, ==, initial); // no-consumer discard
    munit_assert_true(f.queue.attach(0)); munit_assert_false(f.queue.attach(1));
    f.push(MediaTrack::Video, 0); munit_assert_size(f.queue.stats().queued, ==, 0);
    f.push(MediaTrack::Audio, -20000); f.push(MediaTrack::Audio, 0);
    f.push(MediaTrack::Video, 10000, VideoAccess::IDR);
    munit_assert_true(f.queue.stats().ready);
    auto config = f.queue.configuration(); munit_assert_true(config.has_value());
    munit_assert_uint(config->header->data()[4], ==, 0x67);
    munit_assert_null(config->video.header);
    auto packet = f.queue.pop(100000); munit_assert_true(packet.has_value());
    munit_assert_uint64(config->epoch, ==, packet->epoch);
    munit_assert_true(packet->track == MediaTrack::Audio);
    munit_assert_int64(packet->timing.ptsUs, ==, 0); // overlap, not separate zero origins
    munit_assert_uint(packet->buffer->data()[0], ==, 9); munit_assert_null(packet->metadata.data);
    return MUNIT_OK;
}
static MunitResult overflow_and_leases(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0);
    f.push(MediaTrack::Audio, 0); f.push(MediaTrack::Video, 0, VideoAccess::IDR, 1, PacketQueue::MaxPacket);
    auto held = f.queue.pop(100000); munit_assert_true(held.has_value()); // video tie first
    auto bytesWithLease = f.queue.stats().bytes;
    munit_assert_size(bytesWithLease, >, PacketQueue::MaxPacket);
    for(int i = 1; i < 5 && f.queue.stats().connected; ++i)
        f.push(MediaTrack::Video, i * 16000, VideoAccess::Predicted, 1, PacketQueue::MaxPacket);
    auto stats = f.queue.stats();
    munit_assert_false(stats.connected); munit_assert_false(stats.ready);
    munit_assert_true(stats.reason == PacketQueue::Reason::Overflow);
    munit_assert_size(stats.queued, ==, 0); munit_assert_size(stats.bytes, <=, PacketQueue::MaxBytes);
    munit_assert_size(stats.bytes, >, PacketQueue::MaxPacket); // reset cannot uncharge leased bytes
    held.reset(); munit_assert_size(f.queue.stats().bytes, <, PacketQueue::MaxPacket);
    munit_assert_true(f.queue.attach(200000));
    f.push(MediaTrack::Video, 100000); munit_assert_size(f.queue.stats().queued, ==, 0); // no predicted continuation
    return MUNIT_OK;
}
static MunitResult stale_epochs_and_configs(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0); f.push(MediaTrack::Audio,0); f.push(MediaTrack::Video,0,VideoAccess::IDR);
    auto held = f.queue.pop(100000); auto epoch = held->epoch;
    munit_assert_true(f.push(MediaTrack::Video,20000,VideoAccess::Predicted,0) == PacketQueue::Result::Stale);
    ChiakiEncodedAudioConfig audio{2,2,48000,960};
    f.queue.configureAudio(audio);
    munit_assert_false(f.queue.stats().connected);
    munit_assert_true(f.queue.stats().epoch != epoch);
    munit_assert_true(f.queue.stats().reason == PacketQueue::Reason::ConfigurationChanged);
    f.queue.attach(100000);
    munit_assert_true(f.push(MediaTrack::Audio,20000,VideoAccess::Predicted,1) == PacketQueue::Result::Stale);
    f.push(MediaTrack::Video,40000,VideoAccess::CRA);
    munit_assert_true(f.push(MediaTrack::Video,20000,VideoAccess::Leading) == PacketQueue::Result::Dropped);
    f.push(MediaTrack::Audio,40000,VideoAccess::Predicted,2);
    munit_assert_true(f.queue.stats().ready);
    return MUNIT_OK;
}
static MunitResult age_and_initialization(const MunitParameter[], void *) {
    PacketQueue missing; missing.attach(0); missing.maintain(250001);
    munit_assert_false(missing.stats().connected);
    munit_assert_true(missing.stats().reason == PacketQueue::Reason::MissingTrack);
    Fixture f; f.queue.attach(0); f.queue.maintain(5000001);
    munit_assert_true(f.queue.stats().reason == PacketQueue::Reason::KeyframeTimeout);
    f.queue.attach(0); f.push(MediaTrack::Audio,0); f.push(MediaTrack::Video,0,VideoAccess::IDR);
    f.queue.maintain(350001);
    munit_assert_true(f.queue.stats().reason == PacketQueue::Reason::StaleMedia);
    return MUNIT_OK;
}
static MunitResult preroll_and_holes(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0);
    for(int i = 0; i < 100; ++i) f.push(MediaTrack::Audio,i*20000,VideoAccess::Predicted,1,3,uint64_t(i)*20000);
    munit_assert_true(f.queue.stats().connected); munit_assert_size(f.queue.stats().queued, <=, 14);
    f.push(MediaTrack::Video,1990000,VideoAccess::IDR,1,3,1990000);
    f.push(MediaTrack::Audio,2000000,VideoAccess::Predicted,1,0,2000000,CHIAKI_ENCODED_MISSING);
    munit_assert_true(f.queue.stats().ready);
    bool sawHole = false;
    while(auto packet = f.queue.pop(2000000, true)) {
        if(packet->metadata.flags & CHIAKI_ENCODED_MISSING) {
            sawHole = true; munit_assert_size(packet->buffer->size(), ==, 0);
            munit_assert_uint(packet->metadata.samples, ==, 960);
        }
    }
    munit_assert_true(sawHole);
    return MUNIT_OK;
}
static MunitResult bounds_and_timestamps(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0);
    uint8_t byte = 1;
    ChiakiEncodedPacket huge{1,10000,0,0,0,0,&byte,PacketQueue::MaxPacket+1};
    munit_assert_true(f.queue.push(MediaTrack::Video,huge,{0,0},VideoAccess::IDR,10000) == PacketQueue::Result::Invalid);
    munit_assert_size(f.queue.stats().bytes, <, PacketQueue::MaxPacket);
    f.queue.attach(0); f.push(MediaTrack::Audio,0); f.push(MediaTrack::Video,0,VideoAccess::IDR);
    ChiakiEncodedPacket bad{1,10000,1,0,0,0,&byte,1};
    munit_assert_true(f.queue.push(MediaTrack::Video,bad,{20000,10000},VideoAccess::Predicted,10000) == PacketQueue::Result::Reset);
    munit_assert_true(f.queue.stats().reason == PacketQueue::Reason::Timestamp);
    Fixture live(3*1024*1024); live.queue.attach(0); live.push(MediaTrack::Audio,0);
    munit_assert_true(live.push(MediaTrack::Video,0,VideoAccess::IDR,1,PacketQueue::MaxPacket) == PacketQueue::Result::Reset);
    munit_assert_size(live.queue.stats().bytes, <=, 3*1024*1024);
    return MUNIT_OK;
}
static MunitResult concurrent_caps(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0); f.push(MediaTrack::Audio,0); f.push(MediaTrack::Video,0,VideoAccess::IDR);
    std::thread video([&] { for(int i=1;i<300;++i) f.push(MediaTrack::Video,i*1000); });
    std::thread audio([&] { for(int i=1;i<300;++i) f.push(MediaTrack::Audio,i*1000); });
    video.join(); audio.join();
    munit_assert_size(f.queue.stats().entries, <=, PacketQueue::MaxEntries);
    munit_assert_size(f.queue.stats().bytes, <=, PacketQueue::MaxBytes);
    munit_assert_false(f.queue.stats().connected);
    return MUNIT_OK;
}
static MunitResult initial_config(const MunitParameter[], void *) {
    PacketQueue queue; queue.attach(0);
    uint8_t header[]{0,0,0,1,0x67};
    ChiakiEncodedVideoConfig video{1,CHIAKI_CODEC_H264,1920,1080,60,1,header,sizeof(header)};
    ChiakiEncodedAudioConfig audio{1,2,48000,960};
    queue.configureVideo(video); queue.configureAudio(audio);
    munit_assert_true(queue.stats().connected); // first headers are initialization, not replacement
    return MUNIT_OK;
}
static MunitResult preroll_caps(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0);
    for(int i=0;i<10;++i) f.push(MediaTrack::Audio,i*20000,VideoAccess::Predicted,1,PacketQueue::MaxPacket,uint64_t(i)*20000);
    munit_assert_true(f.queue.stats().connected); // bounded audio pre-roll can trim itself
    munit_assert_size(f.queue.stats().queued, <=, 3);
    munit_assert_size(f.queue.stats().bytes, <=, PacketQueue::MaxBytes);
    return MUNIT_OK;
}
static MunitResult admission_before_copy(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0); f.push(MediaTrack::Audio,0); f.push(MediaTrack::Video,0,VideoAccess::IDR);
    std::vector<QueuedPacket> held;
    while(auto packet = f.queue.pop(100000,true)) held.push_back(std::move(*packet));
    int64_t pts = 16000;
    while(f.queue.stats().entries < PacketQueue::MaxEntries) {
        munit_assert_true(f.push(MediaTrack::Video,pts,VideoAccess::Predicted,1,1) == PacketQueue::Result::Accepted);
        auto packet = f.queue.pop(100000,true); munit_assert_true(packet.has_value());
        held.push_back(std::move(*packet)); pts += 16000;
    }
    // Only leases occupy the budget. A claimed small packet must be denied
    // before touching this deliberately invalid pointer, not copied then capped.
    ChiakiEncodedPacket packet{1,100000,1,0,0,0,reinterpret_cast<const uint8_t *>(1),1};
    munit_assert_true(f.queue.push(MediaTrack::Video,packet,{pts,pts},VideoAccess::Predicted,100000) == PacketQueue::Result::Reset);
    munit_assert_true(f.queue.stats().reason == PacketQueue::Reason::Overflow);
    held.clear(); munit_assert_size(f.queue.stats().entries, ==, 2);
    return MUNIT_OK;
}
static MunitResult lease_outlives_queue(const MunitParameter[], void *) {
    std::optional<QueuedPacket> packet; std::optional<QueueConfiguration> config;
    {
        Fixture f; f.queue.attach(0); f.push(MediaTrack::Audio,0); f.push(MediaTrack::Video,0,VideoAccess::IDR);
        packet = f.queue.pop(100000); config = f.queue.configuration();
    }
    munit_assert_true(packet.has_value()); munit_assert_true(config.has_value());
    munit_assert_uint(packet->buffer->data()[0], ==, 9);
    munit_assert_uint(config->header->data()[4], ==, 0x67);
    packet.reset(); config.reset(); // budget and bytes release after owner destruction
    return MUNIT_OK;
}
static MunitResult stale_preroll_cannot_be_ready(const MunitParameter[], void *) {
    Fixture f; f.queue.attach(0);
    f.push(MediaTrack::Audio,0,VideoAccess::Predicted,1,3,0);
    f.push(MediaTrack::Video,10000,VideoAccess::IDR,1,3,300000);
    munit_assert_false(f.queue.stats().ready); // an overlapping PTS is not fresh audio
    munit_assert_size(f.queue.stats().queued, ==, 1);
    return MUNIT_OK;
}
static MunitResult missing_track_details(const MunitParameter[], void *) {
    using Stage=PacketQueue::MissingStage;
    PacketQueue unconfigured; unconfigured.attach(0); unconfigured.maintain(250001);
    munit_assert_true(unconfigured.stats().missing.stage==Stage::Configuration);
    Fixture startup; startup.queue.attach(0);
    startup.push(MediaTrack::Video,1000000,VideoAccess::IDR,1,3,100000);
    startup.push(MediaTrack::Audio,1300000,VideoAccess::Predicted,1,3,100000);
    startup.queue.maintain(350001);
    auto initial=startup.queue.stats();
    munit_assert_true(initial.reason==PacketQueue::Reason::MissingTrack);
    munit_assert_true(initial.missing.stage==Stage::StartupAlignment);
    munit_assert_true(initial.missing.audioStartDeltaUs.has_value());
    munit_assert_int64(*initial.missing.audioStartDeltaUs,==,300000);
    munit_assert_uint64(initial.missing.audioAgeUs,==,250001);
    // Preserve the reset-time sample despite subsequent wall-clock progress.
    startup.queue.maintain(900000);
    munit_assert_uint64(startup.queue.stats().missing.audioAgeUs,==,250001);
    startup.queue.attach(1000000);
    munit_assert_true(startup.queue.stats().missing.stage==Stage::None);
    return MUNIT_OK;
}
static MunitResult missing_track_delivery_details(const MunitParameter[], void *) {
    using Stage=PacketQueue::MissingStage;
    for(auto absent:{Stage::Audio,Stage::Video,Stage::Both}) {
        Fixture f; f.queue.attach(0);
        f.push(MediaTrack::Video,0,VideoAccess::IDR,1,3,0);
        f.push(MediaTrack::Audio,0,VideoAccess::Predicted,1,3,0);
        while(f.queue.pop(0,true)) {}
        // Keep the other track fresh and drained; no stale queued media masks
        // the receipt-age check, and no real-time sleep is needed.
        if(absent!=Stage::Both) {
            auto present=absent==Stage::Audio?MediaTrack::Video:MediaTrack::Audio;
            f.push(present,200000,VideoAccess::Predicted,1,3,200000);
            while(f.queue.pop(200000,true)) {}
        }
        f.queue.maintain(250001);
        auto state=f.queue.stats();
        munit_assert_true(state.reason==PacketQueue::Reason::MissingTrack);
        munit_assert_true(state.missing.stage==absent);
        munit_assert_uint64(state.missing.audioAgeUs,==,absent==Stage::Video?50001:250001);
        munit_assert_uint64(state.missing.videoAgeUs,==,absent==Stage::Audio?50001:250001);
    }
    return MUNIT_OK;
}
int main(int argc,char **argv) {
    MunitTest tests[] = {
        {const_cast<char *>("/startup-owned"), startup_and_owned, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/overflow-leases"), overflow_and_leases, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/epochs-configs"), stale_epochs_and_configs, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/age-initialization"), age_and_initialization, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/preroll-holes"), preroll_and_holes, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/bounds-timestamps"), bounds_and_timestamps, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/concurrent-caps"), concurrent_caps, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/initial-config"), initial_config, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/preroll-caps"), preroll_caps, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/admission-before-copy"), admission_before_copy, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/lease-outlives-queue"), lease_outlives_queue, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/stale-preroll-readiness"), stale_preroll_cannot_be_ready, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/missing-track-details"), missing_track_details, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {const_cast<char *>("/missing-track-delivery-details"), missing_track_delivery_details, nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr},
        {nullptr,nullptr,nullptr,nullptr,MUNIT_TEST_OPTION_NONE,nullptr}};
    MunitSuite suite{const_cast<char *>("/queue"),tests,nullptr,1,MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&suite,nullptr,argc,argv);
}
