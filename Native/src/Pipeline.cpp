#include "Pipeline.h"
#include "AnnexB.h"
#include "MatroskaMuxer.h"
#include "Diagnostics.h"
#include <chiaki/time.h>
#include <atomic>
#include <thread>
#include <cstring>
#include <poll.h>
namespace capture {
struct MediaPipeline::Impl {
    capture_options options;
    LocalTCPServer &server;
    std::function<void()> request;
    VideoFormatParser parseFormat;
    PacketQueue queue{3*1024*1024}; // reserves headroom for core, parser, packet, cluster and AVIO storage
    std::mutex mutex;
    std::atomic<bool> stopping{false},failed{false},hasOBS{false},outputReady{false};
    bool retry=false,formatPending=false,parsing=false,frozen=false;
    capture_error error=CAPTURE_ERROR_NONE;
    const char *message="";
    uint64_t videoCount=0,audioCount=0,videoGeneration=0,audioGeneration=0,signature=0;
    uint64_t videoRevision=0;
    std::optional<uint16_t> lastSourceVideoIndex;
    ChiakiEncodedVideoConfig videoConfig{};
    std::vector<uint8_t> videoHeader;
    Parameters hdrState;
    AVRational lastFrameRate{60,1};
    uint64_t videoLosses=0,audioHoles=0;
    uint64_t outputDisconnects=0;
    std::string lastOutputMessage;
    uint64_t firstHeaderUs=0,originUs=0,lastVideoUs=0,lastAudioUs=0;
    ChiakiEncodedAudioConfig audio{};
    std::optional<ChiakiEncodedPacket> firstVideo,firstAudio;
    struct Candidate { ChiakiEncodedPacket metadata; std::vector<uint8_t> bytes; };
    std::unique_ptr<Candidate> startup;
    std::shared_ptr<VideoFormat> format;
    std::unique_ptr<Timeline> clock;
    std::thread worker;
    Impl(capture_options o,LocalTCPServer &s,std::function<void()> r,VideoFormatParser parser):options(o),server(s),request(std::move(r)),parseFormat(std::move(parser)) {
        if(options.codec!=CAPTURE_H264) {hdrState.reset(avcodec_parameters_alloc());if(!hdrState)throw std::bad_alloc();}
        worker=std::thread([this]{try{run();}catch(...){fail(CAPTURE_ERROR_INTERNAL,"Local output worker failed safely.");server.disconnect();hasOBS=false;outputReady=false;}});
    }
    void failLocked(capture_error e,const char *m,bool canRetry=false) {
        if(failed) return;
        error=e; message=m; retry=canRetry; failed=true;
    }
    void fail(capture_error e,const char *m,bool canRetry=false) { std::lock_guard<std::mutex> lock(mutex); failLocked(e,m,canRetry); }
    void tickClock(uint64_t now) {
        if(!clock && format && firstAudio && firstVideo) {
            originUs=std::min(firstAudio->first_receipt_us,firstVideo->first_receipt_us);
            clock=std::make_unique<Timeline>(TimelineConfig{uint32_t(format->frameRate.num),uint32_t(format->frameRate.den),audio.sample_rate,audio.samples_per_frame,0,true},originUs);
            clock->observe(MediaTrack::Video,firstVideo->frame_index,firstVideo->first_receipt_us);
            clock->observe(MediaTrack::Audio,firstAudio->frame_index,firstAudio->first_receipt_us,firstAudio->samples);
        }
        if(clock && !frozen && now>=originUs && now-originUs>=Timeline::ObservationUs) {
            frozen=clock->freeze(now);
            if(!frozen) failLocked(CAPTURE_ERROR_CONNECTION,"Cannot establish both source clocks. Reconnecting.",true);
        }
    }
    void replaceVideoHeader(ChiakiEncodedVideoConfig c,bool inBand=false) {
        if(!c.generation || !c.header || !c.header_size || c.header_size>PacketQueue::MaxHeader ||
           c.codec!=static_cast<ChiakiCodec>(options.codec) || videoRevision==UINT64_MAX)
            throw std::runtime_error("Unsupported source video configuration");
        Parameters metadata;
        if(options.codec!=CAPTURE_H264) {
            metadata.reset(avcodec_parameters_alloc());if(!metadata)throw std::bad_alloc();
            auto updates=readHDRMetadata(c.header,c.header_size);applyHDRMetadata(metadata.get(),updates.get());
            if(inBand && hdrState)applyHDRMetadata(metadata.get(),hdrState.get());
        }
        auto normalized=c;normalized.generation=videoRevision+1;
        auto result=queue.configureVideo(normalized);
        if(result==PacketQueue::Result::Invalid || result==PacketQueue::Result::Reset)
            throw std::runtime_error("Cannot retain supported video configuration");
        ++videoRevision;videoConfig=c;videoConfig.header=nullptr;
        videoHeader.assign(c.header,c.header+c.header_size);signature=parameterSetSignature(c.codec,c.header,c.header_size);
        hdrState=std::move(metadata);format.reset();startup.reset();formatPending=false;
        firstHeaderUs=chiaki_time_now_monotonic_us();
        if(clock && frozen) clock->reconfigureVideo(lastFrameRate.num,lastFrameRate.den);
    }
    void configureVideo(const ChiakiEncodedVideoConfig &c) {
        std::lock_guard<std::mutex> lock(mutex); if(stopping || failed) return;
        if(videoGeneration && c.generation<videoGeneration) return;
        bool same=videoGeneration && c.codec==videoConfig.codec && c.width==videoConfig.width && c.height==videoConfig.height &&
            c.nominal_fps_num==videoConfig.nominal_fps_num && c.nominal_fps_den==videoConfig.nominal_fps_den &&
            c.header && c.header_size==videoHeader.size() && !memcmp(c.header,videoHeader.data(),c.header_size);
        if(videoGeneration && c.generation==videoGeneration && !same) throw std::runtime_error("Conflicting video generation");
        if(!same) replaceVideoHeader(c);
        if(c.generation!=videoGeneration) {
            lastSourceVideoIndex.reset();
            if(same && clock && frozen)clock->reconfigureVideo(lastFrameRate.num,lastFrameRate.den);
        }
        videoGeneration=c.generation;videoConfig.generation=c.generation;
    }
    void configureAudio(const ChiakiEncodedAudioConfig &c) {
        std::lock_guard<std::mutex> lock(mutex); if(stopping || failed) return;
        if(audioGeneration && c.generation<audioGeneration) return;
        liveOpusHead(c);
        if(audioGeneration && (c.channels!=audio.channels || c.sample_rate!=audio.sample_rate || c.samples_per_frame!=audio.samples_per_frame)) {
            failLocked(CAPTURE_ERROR_MEDIA,"Source audio format changed incompatibly. No resampling fallback was applied.");return;
        }
        if(queue.configureAudio(c)==PacketQueue::Result::Invalid) throw std::runtime_error("Unsupported source audio configuration");
        audio=c; audioGeneration=c.generation; if(!firstHeaderUs) firstHeaderUs=chiaki_time_now_monotonic_us();
    }
    bool receive(MediaTrack track,const ChiakiEncodedPacket &p) {
        std::lock_guard<std::mutex> lock(mutex); if(stopping || failed) return true;
        bool isAudio=track==MediaTrack::Audio;
        if(p.generation!=(isAudio?audioGeneration:videoGeneration)) return true;
        uint64_t now=chiaki_time_now_monotonic_us();
        VideoAccess access=VideoAccess::Predicted;
        if(p.size>PacketQueue::MaxPacket || (p.size && !p.data) || p.first_receipt_us>now) throw std::runtime_error("Invalid source packet bounds or timestamp");
        if(isAudio) {
            if(p.flags&CHIAKI_ENCODED_MISSING) ++audioHoles;
            if(!(p.flags&CHIAKI_ENCODED_MISSING)) {
                if(p.size>65536 || opusPacketSamples(p.data,p.size)!=audio.samples_per_frame || p.samples!=audio.samples_per_frame)
                    throw std::runtime_error("Unexpected Opus sample duration");
                ++audioCount; lastAudioUs=p.first_receipt_us;
                if(!clock) { firstAudio=p; firstAudio->data=nullptr; firstAudio->size=0; }
            }
        } else {
            auto info=inspectAccessUnit(static_cast<ChiakiCodec>(options.codec),p.data,p.size);
            // Reject obsolete access units before inspecting their metadata:
            // otherwise a late old SPS/SEI can undo an already accepted update.
            if(lastSourceVideoIndex) {
                auto delta=uint16_t(p.frame_index-*lastSourceVideoIndex);
                if(!delta || delta>0x8000)return true;
                if(delta==0x8000) {failLocked(CAPTURE_ERROR_CONNECTION,"Ambiguous source video counter. Reconnecting.",true);return true;}
            }
            auto current=parameterSetSignature(static_cast<ChiakiCodec>(options.codec),p.data,p.size);
            if(current && current!=signature) {
                auto updated=updatedVideoHeader(static_cast<ChiakiCodec>(options.codec),videoHeader.data(),videoHeader.size(),p.data,p.size);
                if(updated) {auto c=videoConfig;c.header=updated->data();c.header_size=updated->size();replaceVideoHeader(c,true);}
            }
            if(options.codec!=CAPTURE_H264) {
                auto updates=readHDRMetadata(p.data,p.size);
                if(applyHDRMetadata(hdrState.get(),updates.get()) && format) {
                    auto next=std::make_shared<VideoFormat>();next->parameters.reset(avcodec_parameters_alloc());
                    if(!next->parameters || avcodec_parameters_copy(next->parameters.get(),format->parameters.get())<0)throw std::bad_alloc();
                    next->frameRate=format->frameRate;next->depth=format->depth;next->hdr=format->hdr;
                    applyHDRMetadata(next->parameters.get(),hdrState.get());format=std::move(next);
                }
            }
            if(!info.hasPicture) return true;
            lastSourceVideoIndex=p.frame_index;
            access=info.access; ++videoCount; lastVideoUs=p.first_receipt_us;
            videoLosses+=p.lost_frames;
            if(!clock) { firstVideo=p; firstVideo->data=nullptr; firstVideo->size=0; }
            if(!format && !formatPending && !parsing && (access==VideoAccess::IDR || access==VideoAccess::CRA)) {
                auto candidate=std::make_unique<Candidate>(); candidate->metadata=p; candidate->metadata.data=nullptr;candidate->metadata.generation=videoRevision;
                candidate->bytes.assign(p.data,p.data+p.size);
                startup=std::move(candidate); formatPending=true;
            }
        }
        tickClock(now); if(!clock || failed) return true;
        auto observation=clock->observe(track,p.frame_index,p.first_receipt_us,isAudio?p.samples:0,p.flags&CHIAKI_ENCODED_MISSING);
        if(observation.status==Timeline::Status::Discontinuity) { failLocked(CAPTURE_ERROR_CONNECTION,"Source timing discontinuity. Reconnecting both tracks.",true); return true; }
        if(observation.status!=Timeline::Status::Accepted || !frozen) return true;
        auto pts=clock->pts(track,observation.position);
        if(!pts) { failLocked(CAPTURE_ERROR_MEDIA,"Cannot represent source timestamps safely."); return false; }
        auto queued=p;if(!isAudio)queued.generation=videoRevision;
        auto result=queue.push(track,queued,{*pts,*pts},access,now);
        if(result==PacketQueue::Result::Invalid) { failLocked(CAPTURE_ERROR_MEDIA,"Unsupported media packet. No transcoding fallback is available."); return false; }
        return true; // no OBS/overflow is discard+accept, never fake decoder failure
    }
    void prepare() {
        std::unique_ptr<Candidate> candidate;
        // A valid video IDR may precede the independent audio configuration.
        // Keep its bounded startup copy until both configuration callbacks arrive.
        std::optional<QueueConfiguration> config;
        { std::lock_guard<std::mutex> lock(mutex);config=queue.configuration();if(config && startup) {candidate=std::move(startup);parsing=true;} }
        if(candidate) {
            auto revision=candidate->metadata.generation;
            std::shared_ptr<VideoFormat> parsed;
            try {
                if(!config || config->video.generation!=revision)throw std::runtime_error("Missing matching track configuration");
                parsed=std::make_shared<VideoFormat>(parseFormat(static_cast<ChiakiCodec>(options.codec),config->header->data(),config->header->size(),candidate->bytes.data(),candidate->bytes.size()));
            } catch(...) {
                std::lock_guard<std::mutex> lock(mutex);parsing=false;
                if(revision!=videoRevision)return;
                throw;
            }
            candidate.reset(); // parser/startup allocations never overlap the output mux
            std::lock_guard<std::mutex> lock(mutex);parsing=false;
            if(revision==videoRevision) {
                if(hdrState) {applyHDRMetadata(parsed->parameters.get(),hdrState.get());applyHDRMetadata(hdrState.get(),parsed->parameters.get());}
                lastFrameRate=parsed->frameRate;
                if(clock && frozen)clock->reconfigureVideo(lastFrameRate.num,lastFrameRate.den,false);
                format=std::move(parsed);formatPending=false;
            }
        }
        std::lock_guard<std::mutex> lock(mutex);
        auto now=chiaki_time_now_monotonic_us(); tickClock(now);
        if(firstHeaderUs && (!format || !firstAudio || !firstVideo) && now-firstHeaderUs>5000000)
            failLocked(CAPTURE_ERROR_MEDIA,"PS5 did not provide both tracks and usable video headers.");
        if(format && lastAudioUs && lastVideoUs && (now-lastAudioUs>2000000 || now-lastVideoUs>2000000))
            failLocked(CAPTURE_ERROR_CONNECTION,"A required media track stopped arriving. Reconnecting.",true);
    }
    void run() {
        std::unique_ptr<MatroskaMuxer> mux; Parameters audioParameters;
        uint64_t clientEpoch=0,lastIDR=0; int64_t epoch=0; bool wroteAudio=false,wroteVideo=false;
        unsigned overflows=0; int networkError=0;
        auto close=[&](const std::string &reason=std::string{}) {
            if(!reason.empty() && hasOBS && !stopping) {
                std::lock_guard<std::mutex> lock(mutex);
                ++outputDisconnects;lastOutputMessage=reason;
            }
            mux.reset(); audioParameters.reset(); server.disconnect(); hasOBS=false; outputReady=false;
            if(queue.stats().connected) queue.detach();
            wroteAudio=wroteVideo=false;
        };
        while(!stopping && !failed) {
            try { prepare(); } catch(...) { fail(CAPTURE_ERROR_MEDIA,"Unsupported video cadence, reordering, colour, or HDR negotiation. No fallback was applied."); break; }
            if(failed) break;
            if(!hasOBS) {
                bool calibrated,needVideo;
                {std::lock_guard<std::mutex> lock(mutex);calibrated=frozen && bool(format);needVideo=videoGeneration && !format;}
                if(!calibrated) {
                    auto now=chiaki_time_now_monotonic_us();
                    if(needVideo && now-lastIDR>=1000000) {request();lastIDR=now;}
                    poll(nullptr,0,5);continue;
                }
                auto deadline=chiaki_time_now_monotonic_us()+20000;
                if(!server.acceptClient([&]{return stopping || failed || chiaki_time_now_monotonic_us()>=deadline;})) continue;
                hasOBS=true; queue.attach(chiaki_time_now_monotonic_us()); lastIDR=0;
            }
            server.rejectExtraClients();
            if(!server.clientAlive()) { close("Last OBS reset: client closed the local connection."); continue; }
            uint64_t now=chiaki_time_now_monotonic_us(); queue.maintain(now); auto stats=queue.stats();
            if(!stats.connected) {
                if(stats.reason==PacketQueue::Reason::Overflow && ++overflows>=5) fail(CAPTURE_ERROR_OVERFLOW,"Repeated media overflow. Stop and lower the bitrate before retrying.");
                close(outputQueueResetMessage(stats)); continue;
            }
            if(!stats.ready && now-lastIDR>=1000000) { request(); lastIDR=now; }
            auto leased=queue.pop(now);
            if(!leased) { poll(nullptr,0,5); continue; }
            networkError=0;
            try {
                if(!mux) {
                    std::shared_ptr<VideoFormat> current;
                    { std::lock_guard<std::mutex> lock(mutex); current=format; }
                    auto config=queue.configuration();
                    if(!current || !config || config->epoch!=leased->epoch) { close(outputQueueResetMessage(queue.stats())); continue; }
                    clientEpoch=leased->epoch; epoch=leased->timing.ptsUs;
                    audioParameters=liveAudioParameters(config->audio);
                    mux=std::make_unique<MatroskaMuxer>(std::vector<Track>{{current->parameters.get(),AV_TIME_BASE_Q,current->frameRate},{audioParameters.get(),AV_TIME_BASE_Q,{0,1}}},
                        [&](const uint8_t *p,int n) {
                            int rc=server.write(p,n,[&]{return stopping || failed || queue.stats().epoch!=clientEpoch;});
                            if(rc<0) networkError=rc; return rc;
                        },true);
                }
                if(leased->epoch!=clientEpoch || queue.stats().epoch!=clientEpoch) { close(outputQueueResetMessage(queue.stats())); continue; }
                if(leased->metadata.flags&CHIAKI_ENCODED_MISSING) continue;
                auto packet=av_packet_alloc(); if(!packet) throw std::bad_alloc();
                if(av_new_packet(packet,int(leased->buffer->size()))<0) { av_packet_free(&packet); throw std::bad_alloc(); }
                memcpy(packet->data,leased->buffer->data(),leased->buffer->size());
                packet->stream_index=leased->track==MediaTrack::Video?0:1;
                packet->pts=packet->dts=leased->timing.ptsUs-epoch;
                if(leased->access==VideoAccess::IDR || leased->access==VideoAccess::CRA) packet->flags|=AV_PKT_FLAG_KEY;
                if(packet->stream_index==1) packet->duration=int64_t(leased->metadata.samples)*1000000/48000;
                try { mux->write(packet); } catch(...) { av_packet_free(&packet); throw; }
                if(packet->stream_index==0) wroteVideo=true; else wroteAudio=true;
                av_packet_free(&packet); outputReady=wroteVideo && wroteAudio;
            } catch(...) {
                auto state=queue.stats();
                if(!networkError && !stopping && !failed && state.epoch==clientEpoch)
                    fail(CAPTURE_ERROR_MEDIA,"Cannot mux a safe two-track stream. Check codec and timing compatibility.");
                std::string reason;
                if(!stopping && !failed) {
                    reason=(!state.connected || state.epoch!=clientEpoch)
                        ?outputQueueResetMessage(state):outputWriteResetMessage(networkError);
                }
                close(reason);
            }
        }
        close();
    }
};
MediaPipeline::MediaPipeline(capture_options o,LocalTCPServer &s,std::function<void()> r,VideoFormatParser parser):impl_(std::make_unique<Impl>(o,s,std::move(r),std::move(parser))) {}
MediaPipeline::~MediaPipeline() { stopAndJoin(); }
ChiakiEncodedMediaSink MediaPipeline::sink() {
    return {impl_.get(),
        [](const auto *c,void *ctx){auto &p=*static_cast<Impl *>(ctx); try{p.configureVideo(*c);}catch(...){p.fail(CAPTURE_ERROR_MEDIA,"Unsupported or oversized video configuration.");}},
        [](const auto *c,void *ctx){auto &p=*static_cast<Impl *>(ctx); try{p.configureAudio(*c);}catch(...){p.fail(CAPTURE_ERROR_MEDIA,"Only supported stereo 48 kHz Opus audio is accepted.");}},
        [](const auto *c,void *ctx)->bool{auto &p=*static_cast<Impl *>(ctx); try{return p.receive(MediaTrack::Video,*c);}catch(...){p.fail(CAPTURE_ERROR_MEDIA,"Unsupported or oversized encoded video packet.");return false;}},
        [](const auto *c,void *ctx){auto &p=*static_cast<Impl *>(ctx); try{p.receive(MediaTrack::Audio,*c);}catch(...){p.fail(CAPTURE_ERROR_MEDIA,"Audio packet duration or configuration is unsupported.");}}};
}
SessionSnapshot MediaPipeline::snapshot() {
    auto &p=*impl_; auto stats=p.queue.stats(); std::lock_guard<std::mutex> lock(p.mutex);
    SessionSnapshot s; s.done=p.failed; s.retry=p.retry; s.status.state=p.failed?CAPTURE_FAILED:p.outputReady?CAPTURE_STREAMING:CAPTURE_WAITING_OBS;
    s.status.error=p.error; strncpy(s.status.message,p.failed?p.message:p.lastOutputMessage.c_str(),sizeof(s.status.message)-1);
    s.status.video_packets=p.videoCount; s.status.audio_packets=p.audioCount; s.status.queued_bytes=stats.bytes;
    s.status.lost_video_frames=p.videoLosses;s.status.missing_audio_packets=p.audioHoles;
    s.status.output_disconnects=p.outputDisconnects;
    s.status.audio_samples_per_packet=p.audio.samples_per_frame;
    if(p.clock) {
        auto video=p.clock->diagnostics(MediaTrack::Video),audio=p.clock->diagnostics(MediaTrack::Audio);
        s.status.video_clock_drift_us=video.driftUs;s.status.audio_clock_drift_us=audio.driftUs;
        s.status.video_clock_elapsed_us=video.elapsedUs;s.status.audio_clock_elapsed_us=audio.elapsedUs;
        s.status.video_clock_position=video.position;s.status.audio_clock_position=audio.position;
    }
    auto now=chiaki_time_now_monotonic_us();
    s.status.queue_age_us=stats.oldestReceiptUs && now>=stats.oldestReceiptUs?now-stats.oldestReceiptUs:0;
    s.status.codec=p.options.codec;
    if(p.format) { s.status.width=p.format->parameters->width; s.status.height=p.format->parameters->height; s.status.fps_num=p.format->frameRate.num; s.status.fps_den=p.format->frameRate.den; }
    return s;
}
void MediaPipeline::stopAndJoin() { if(impl_) { impl_->stopping=true; if(impl_->worker.joinable()) impl_->worker.join(); } }
}
