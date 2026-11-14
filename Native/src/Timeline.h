#pragma once
#include <cstdint>
#include <optional>
#include <array>

namespace capture {
enum class MediaTrack { Video, Audio };
struct TimelineConfig {
    uint32_t fpsNum = 60, fpsDen = 1;
    uint32_t audioRate = 48000, samplesPerFrame = 960;
    int64_t audioOffsetUs = 0; // Positive delays audio, before common normalization.
    bool audioAnchoredIngressVideo = false; // Explicit live strategy; never infer transport cadence from SPS.
};
class Timeline {
public:
    static constexpr uint64_t ObservationUs = 250000, MaxGapUs = 2000000;
    enum class Status { Accepted, Duplicate, Reordered, Discontinuity };
    struct Observation { Status status; uint64_t position; };
    struct Diagnostics {
        uint64_t position=0,elapsedUs=0;
        int64_t driftUs=0;
    };
    // fps must be independently verified by the caller, not just requested max.
    // Caller serializes this object. origin begins the MEDIA observation window
    // (e.g. stream-info, no later than initial packet entry), not authentication
    // or a potentially slow network handshake. Freeze at the deadline before
    // observing post-window packets; a missing track requires source restart.
    Timeline(TimelineConfig config, uint64_t sessionOriginUs);
    Observation observe(MediaTrack track, uint16_t index, uint64_t firstReceiptUs,
                        uint32_t samples = 0, bool missing = false);
    bool freeze(uint64_t nowUs);
    // In ingress-video mode only the latest video position is queryable. Copy
    // its PTS at admission; subsequent audio observations never rewrite it.
    std::optional<int64_t> pts(MediaTrack track, uint64_t position) const;
    Diagnostics diagnostics(MediaTrack) const;
    bool failed() const { return failed_; }
private:
    struct Clock {
        bool started = false, hasPayload = false;
        uint16_t raw = 0;
        uint64_t position = 0, receiptUs = 0, firstReceiptUs = 0;
        int64_t offsetUs = INT64_MAX;
    } video_, audio_;
    TimelineConfig config_;
    uint64_t originUs_;
    bool frozen_ = false, failed_ = false;
    struct DelayBucket { uint64_t epoch=UINT64_MAX; int64_t delayUs=0; };
    static constexpr uint64_t DelayBucketUs=25000;
    std::array<DelayBucket,16> audioDelays_{};
    uint64_t audioDelayReceiptUs_=0;
    std::optional<int64_t> ingressVideoPts_;
    int64_t mediaTime(MediaTrack track, uint64_t position) const;
    void rememberAudioDelay(uint64_t receipt, int64_t mediaUs);
    bool updateIngressVideoPts();
};
}
