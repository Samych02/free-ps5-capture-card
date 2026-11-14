#include "Timeline.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace capture {
Timeline::Timeline(TimelineConfig config, uint64_t origin) : config_(config), originUs_(origin) {
    if(!config.fpsNum || !config.fpsDen || config.fpsNum > 1000000 || config.fpsDen > 1000000 ||
       uint64_t(config.fpsNum) > uint64_t(config.fpsDen) * 240 || !config.audioRate || config.audioRate > 192000 ||
       !config.samplesPerFrame || config.samplesPerFrame > config.audioRate ||
       config.audioOffsetUs < -250000 || config.audioOffsetUs > 250000 || origin > uint64_t(INT64_MAX) - MaxGapUs)
        throw std::invalid_argument("Invalid verified media clock configuration");
}
int64_t Timeline::mediaTime(MediaTrack track, uint64_t position) const {
    __int128 value = position;
    if(track == MediaTrack::Video) value = value * 1000000 * config_.fpsDen / config_.fpsNum;
    else value = value * config_.samplesPerFrame * 1000000 / config_.audioRate;
    if(value > INT64_MAX) throw std::overflow_error("Media clock overflow");
    return static_cast<int64_t>(value);
}
Timeline::Observation Timeline::observe(MediaTrack track, uint16_t index, uint64_t receipt, uint32_t samples, bool missing) {
    auto &clock = track == MediaTrack::Video ? video_ : audio_;
    auto fail = [&]() { failed_ = true; return Observation{Status::Discontinuity, clock.position}; };
    if(failed_ || receipt < originUs_ || receipt > uint64_t(INT64_MAX) ||
       (track == MediaTrack::Audio && samples != config_.samplesPerFrame) ||
       (track == MediaTrack::Video && (missing || samples))) return fail();
    if(!frozen_ && receipt - originUs_ > ObservationUs) return fail();
    uint64_t nextPosition = 0;
    if(clock.started) {
        if(receipt > clock.receiptUs && receipt - clock.receiptUs > MaxGapUs) return fail();
        uint16_t delta = uint16_t(index - clock.raw);
        if(!delta) return {Status::Duplicate, clock.position};
        if(delta == 0x8000) return fail(); // exactly half a counter is ambiguous
        if(delta > 0x8000) return {Status::Reordered, clock.position};
        uint64_t maxFrames = track == MediaTrack::Video
            ? MaxGapUs * config_.fpsNum / (1000000 * uint64_t(config_.fpsDen))
            : MaxGapUs * config_.audioRate / (1000000 * uint64_t(config_.samplesPerFrame));
        if(delta > maxFrames ||
           (receipt < clock.receiptUs && clock.receiptUs - receipt > ObservationUs) ||
           clock.position > UINT64_MAX - delta) return fail();
        nextPosition = clock.position + delta;
    }
    int64_t mediaUs;
    try { mediaUs = mediaTime(track, nextPosition); } catch(const std::overflow_error &) { return fail(); }
    if(!clock.started) clock.firstReceiptUs=receipt;
    clock.started = true; clock.raw = index; clock.position = nextPosition;
    clock.receiptUs = std::max(clock.receiptUs, receipt);
    if(!missing) {
        clock.hasPayload = true;
        if(!frozen_) clock.offsetUs = std::min(clock.offsetUs, int64_t(receipt - originUs_) - mediaUs);
    }
    return {Status::Accepted, nextPosition};
}
bool Timeline::freeze(uint64_t now) {
    if(failed_) return false;
    if(frozen_) return true;
    if(now < originUs_ || now - originUs_ < ObservationUs) return false;
    if(!video_.hasPayload || !audio_.hasPayload) { failed_ = true; return false; }
    frozen_ = true;
    return true;
}
std::optional<int64_t> Timeline::pts(MediaTrack track, uint64_t position) const {
    const auto &clock = track == MediaTrack::Video ? video_ : audio_;
    if(!frozen_ || failed_ || !clock.hasPayload || position > clock.position) return std::nullopt;
    __int128 value;
    try { value = mediaTime(track, position); } catch(const std::overflow_error &) { return std::nullopt; }
    value += clock.offsetUs;
    if(track == MediaTrack::Audio) value += config_.audioOffsetUs;
    if(value < INT64_MIN || value > INT64_MAX) return std::nullopt;
    return static_cast<int64_t>(value);
}
Timeline::Diagnostics Timeline::diagnostics(MediaTrack track) const {
    const auto &clock=track==MediaTrack::Video?video_:audio_;
    if(!clock.started) return {};
    Diagnostics result{clock.position,clock.receiptUs-clock.firstReceiptUs,0};
    try {result.driftUs=mediaTime(track,clock.position)-int64_t(result.elapsedUs);}
    catch(const std::overflow_error &) {result.driftUs=INT64_MAX;}
    return result;
}
}
