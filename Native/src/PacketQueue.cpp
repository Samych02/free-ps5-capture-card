#include "PacketQueue.h"
#include <algorithm>
#include <cstring>
namespace capture {
struct QueueBudget {
    std::atomic<size_t> bytes{sizeof(PacketQueue) + sizeof(QueueBudget) + 128};
    std::atomic<size_t> buffers{0};
};
QueuedBuffer::QueuedBuffer(std::shared_ptr<QueueBudget> budget, const uint8_t *data, size_t size, size_t charge)
    : budget_(std::move(budget)), size_(size), charge_(charge) {
    if(size) { bytes_ = std::make_unique<uint8_t[]>(size); memcpy(bytes_.get(), data, size); }
}
QueuedBuffer::~QueuedBuffer() {
    // Free payload before returning admission headroom to another producer.
    bytes_.reset();
    budget_->bytes.fetch_sub(charge_, std::memory_order_relaxed);
    budget_->buffers.fetch_sub(1, std::memory_order_relaxed);
}
PacketQueue::PacketQueue() : budget_(std::make_shared<QueueBudget>()) {}
bool PacketQueue::canCopy(size_t size) const {
    size_t charge = size + sizeof(QueuedBuffer) + 128;
    return charge <= MaxBytes && budget_->buffers.load() < MaxEntries - 1 && budget_->bytes.load() <= MaxBytes - charge;
}
std::shared_ptr<const QueuedBuffer> PacketQueue::copy(const uint8_t *data, size_t size) {
    // Includes owned data/metadata plus a conservative control-block allowance;
    // fixed queue slots and budget metadata are charged in the base. This is an
    // owned-storage budget, not a claim about allocator RSS or FFmpeg buffering.
    size_t charge = size + sizeof(QueuedBuffer) + 128;
    if(!canCopy(size)) return {};
    budget_->bytes.fetch_add(charge); budget_->buffers.fetch_add(1);
    try { return std::make_shared<QueuedBuffer>(budget_, data, size, charge); }
    catch(...) { budget_->bytes.fetch_sub(charge); budget_->buffers.fetch_sub(1); return {}; }
}
size_t PacketQueue::queued() const {
    size_t count = 0; for(const auto &packet : packets_) count += packet.has_value(); return count;
}
void PacketQueue::clearPackets() { for(auto &packet : packets_) packet.reset(); }
void PacketQueue::reset(Reason reason) {
    clearPackets(); connected_ = false; ready_ = false; discardLeading_ = false;
    startPts_.reset(); lastVideoPts_.reset(); lastAudioPts_.reset(); reason_ = reason; ++epoch_;
}
PacketQueue::Result PacketQueue::configureVideo(const ChiakiEncodedVideoConfig &video) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!video.generation || !video.header || !video.header_size || video.header_size > MaxHeader ||
       !video.width || !video.height || video.width > 1920 || video.height > 1080 ||
       (video.codec != CHIAKI_CODEC_H264 && video.codec != CHIAKI_CODEC_H265 && video.codec != CHIAKI_CODEC_H265_HDR)) {
        reset(Reason::InvalidData); return Result::Invalid;
    }
    if(hasVideo_ && video.generation < config_.video.generation) return Result::Stale;
    if(hasVideo_ && video.generation == config_.video.generation) {
        if(video.codec == config_.video.codec && video.width == config_.video.width && video.height == config_.video.height &&
           video.nominal_fps_num == config_.video.nominal_fps_num && video.nominal_fps_den == config_.video.nominal_fps_den &&
           video.header_size == config_.header->size() && !memcmp(video.header, config_.header->data(), video.header_size)) return Result::Dropped;
        reset(Reason::InvalidData); return Result::Invalid;
    }
    if(connected_ && hasVideo_) reset(Reason::ConfigurationChanged);
    auto header = copy(video.header, video.header_size);
    if(!header) { reset(Reason::Overflow); return Result::Reset; }
    config_.video = video; config_.video.header = nullptr; config_.header = std::move(header); hasVideo_ = true;
    return Result::Accepted;
}
PacketQueue::Result PacketQueue::configureAudio(const ChiakiEncodedAudioConfig &audio) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!audio.generation || audio.channels != 2 || audio.sample_rate != 48000 || !audio.samples_per_frame || audio.samples_per_frame > 5760) {
        reset(Reason::InvalidData); return Result::Invalid;
    }
    if(hasAudio_ && audio.generation < config_.audio.generation) return Result::Stale;
    if(hasAudio_ && audio.generation == config_.audio.generation) {
        if(audio.channels == config_.audio.channels && audio.sample_rate == config_.audio.sample_rate &&
           audio.samples_per_frame == config_.audio.samples_per_frame) return Result::Dropped;
        reset(Reason::InvalidData); return Result::Invalid;
    }
    if(connected_ && hasAudio_) reset(Reason::ConfigurationChanged);
    config_.audio = audio; hasAudio_ = true; return Result::Accepted;
}
bool PacketQueue::attach(uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(connected_) return false;
    clearPackets(); connected_ = true; ready_ = false; discardLeading_ = false;
    attachedUs_ = now; lastVideoReceiptUs_ = now; lastAudioReceiptUs_ = now;
    startPts_.reset(); lastVideoPts_.reset(); lastAudioPts_.reset(); reason_ = Reason::None; ++epoch_;
    return true;
}
void PacketQueue::detach() { std::lock_guard<std::mutex> lock(mutex_); reset(Reason::Detached); }
void PacketQueue::pruneAudio(int64_t boundary) {
    int64_t duration = int64_t(config_.audio.samples_per_frame) * 1000000 / config_.audio.sample_rate;
    for(auto &packet : packets_)
        if(packet && packet->track == MediaTrack::Audio && __int128(packet->timing.ptsUs) + duration <= boundary) packet.reset();
}
void PacketQueue::updateReady() {
    if(ready_ || !startPts_ || !hasVideo_ || !hasAudio_) return;
    int64_t duration = int64_t(config_.audio.samples_per_frame) * 1000000 / config_.audio.sample_rate;
    for(const auto &packet : packets_) {
        if(packet && packet->track == MediaTrack::Audio && !(packet->metadata.flags & CHIAKI_ENCODED_MISSING) &&
           __int128(packet->timing.ptsUs) + duration > *startPts_ && __int128(packet->timing.ptsUs) - *startPts_ <= MaxAgeUs) {
            ready_ = true; return;
        }
    }
}
void PacketQueue::maintainLocked(uint64_t now) {
    if(!connected_) return;
    if(now < attachedUs_) { reset(Reason::Timestamp); return; }
    if((!hasVideo_ || !hasAudio_) && now - attachedUs_ > MaxAgeUs) { reset(Reason::MissingTrack); return; }
    if(!ready_) {
        if(startPts_ && now >= startReceiptUs_ && now - startReceiptUs_ > MaxAgeUs) reset(Reason::MissingTrack);
        else if(!startPts_ && now - attachedUs_ > KeyframeWaitUs) reset(Reason::KeyframeTimeout);
        return;
    }
    for(const auto &packet : packets_)
        if(packet && now >= packet->metadata.first_receipt_us && now - packet->metadata.first_receipt_us > MaxAgeUs) {
            reset(Reason::StaleMedia); return;
        }
    if((now >= lastVideoReceiptUs_ && now - lastVideoReceiptUs_ > MaxAgeUs) ||
       (now >= lastAudioReceiptUs_ && now - lastAudioReceiptUs_ > MaxAgeUs)) reset(Reason::MissingTrack);
}
void PacketQueue::maintain(uint64_t now) { std::lock_guard<std::mutex> lock(mutex_); maintainLocked(now); }
PacketQueue::Result PacketQueue::push(MediaTrack track, const ChiakiEncodedPacket &packet, PacketTiming timing, VideoAccess access, uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool audio = track == MediaTrack::Audio;
    bool missing = packet.flags & CHIAKI_ENCODED_MISSING;
    if((audio && hasAudio_ && packet.generation != config_.audio.generation) ||
       (!audio && hasVideo_ && packet.generation != config_.video.generation)) return Result::Stale;
    if(packet.size > MaxPacket || (packet.size && !packet.data) || (!packet.size && !missing) ||
       (missing && (!audio || packet.size)) || (packet.flags & ~7u) || packet.first_receipt_us > now ||
       (audio && hasAudio_ && packet.samples != config_.audio.samples_per_frame) || (!audio && packet.samples)) {
        reset(Reason::InvalidData); return Result::Invalid;
    }
    if(!connected_) return Result::Dropped;
    maintainLocked(now);
    if(!connected_) return Result::Reset;
    if(!hasVideo_ || !hasAudio_) return Result::Dropped;
    if(!audio && access == VideoAccess::Leading && discardLeading_) return Result::Dropped;
    auto &lastPts = audio ? lastAudioPts_ : lastVideoPts_;
    if(timing.ptsUs != timing.dtsUs || (lastPts && timing.ptsUs <= *lastPts)) { reset(Reason::Timestamp); return Result::Reset; }
    if(now - packet.first_receipt_us > MaxAgeUs) {
        if(ready_) { reset(Reason::StaleMedia); return Result::Reset; }
        return Result::Dropped;
    }
    if(!audio && !startPts_ && access != VideoAccess::IDR && access != VideoAccess::CRA) return Result::Dropped;
    if(!ready_ && audio && !startPts_) {
        auto boundary = __int128(timing.ptsUs) - MaxAgeUs;
        pruneAudio(boundary < INT64_MIN ? INT64_MIN : int64_t(boundary));
    }
    if(!ready_) {
        for(auto &queued : packets_)
            if(queued && queued->track == MediaTrack::Audio && now >= queued->metadata.first_receipt_us &&
               now - queued->metadata.first_receipt_us > MaxAgeUs) queued.reset();
    }
    if(!audio && !startPts_) {
        startPts_ = timing.ptsUs; startReceiptUs_ = packet.first_receipt_us;
        discardLeading_ = access == VideoAccess::CRA;
        pruneAudio(timing.ptsUs);
    }
    if(!ready_ && audio && !startPts_) {
        while(!canCopy(packet.size)) {
            size_t oldest = packets_.size();
            for(size_t i = 0; i < packets_.size(); ++i)
                if(packets_[i] && packets_[i]->track == MediaTrack::Audio &&
                   (oldest == packets_.size() || packets_[i]->timing.ptsUs < packets_[oldest]->timing.ptsUs)) oldest = i;
            if(oldest == packets_.size()) break;
            packets_[oldest].reset(); // intentional rolling pre-roll, no predicted dependencies
        }
    }
    auto data = copy(packet.data, packet.size);
    if(!data) { reset(Reason::Overflow); return Result::Reset; }
    for(auto &slot : packets_) if(!slot) {
        slot = QueuedPacket{track,packet,timing,access,epoch_,std::move(data)};
        slot->metadata.data = nullptr;
        lastPts = timing.ptsUs;
        if(audio) lastAudioReceiptUs_ = std::max(lastAudioReceiptUs_, packet.first_receipt_us);
        else lastVideoReceiptUs_ = std::max(lastVideoReceiptUs_, packet.first_receipt_us);
        updateReady(); return Result::Accepted;
    }
    reset(Reason::Overflow); return Result::Reset;
}
std::optional<QueuedPacket> PacketQueue::pop(uint64_t now, bool drain) {
    std::lock_guard<std::mutex> lock(mutex_); maintainLocked(now);
    if(!connected_ || !ready_) return {};
    bool video = false, audio = false;
    size_t best = packets_.size();
    for(size_t i = 0; i < packets_.size(); ++i) if(packets_[i]) {
        const auto &packet = *packets_[i];
        video |= packet.track == MediaTrack::Video; audio |= packet.track == MediaTrack::Audio;
        if(best == packets_.size() || packet.timing.dtsUs < packets_[best]->timing.dtsUs ||
           (packet.timing.dtsUs == packets_[best]->timing.dtsUs && packet.track == MediaTrack::Video)) best = i;
    }
    // Keep one track's look-ahead rather than giving an output muxer unbounded
    // interleaving work. Only an explicit finite-stream drain bypasses this.
    if(best == packets_.size() || (!drain && (!video || !audio))) return {};
    auto result = std::move(packets_[best]); packets_[best].reset(); return result;
}
std::optional<QueueConfiguration> PacketQueue::configuration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!hasVideo_ || !hasAudio_) return {};
    auto snapshot = config_;
    snapshot.epoch = epoch_;
    return snapshot;
}
PacketQueue::Stats PacketQueue::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {budget_->bytes.load(), budget_->buffers.load()+1, queued(), epoch_, connected_, ready_, reason_};
}
}
