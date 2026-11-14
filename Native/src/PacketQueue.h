#pragma once
#include "Timeline.h"
#include <chiaki/encodedmedia.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

namespace capture {
enum class VideoAccess { Predicted, IDR, CRA, Leading };
struct QueueBudget;
class QueuedBuffer {
public:
    QueuedBuffer(std::shared_ptr<QueueBudget>, const uint8_t *, size_t, size_t);
    ~QueuedBuffer();
    const uint8_t *data() const { return bytes_.get(); }
    size_t size() const { return size_; }
private:
    std::shared_ptr<QueueBudget> budget_;
    std::unique_ptr<uint8_t[]> bytes_;
    size_t size_, charge_;
};
struct PacketTiming { int64_t ptsUs, dtsUs; };
struct QueuedPacket {
    MediaTrack track;
    ChiakiEncodedPacket metadata{}; // data pointer cleared; use buffer
    PacketTiming timing{};
    VideoAccess access = VideoAccess::Predicted;
    uint64_t epoch = 0;
    std::shared_ptr<const QueuedBuffer> buffer;
};
struct QueueConfiguration {
    uint64_t epoch = 0;
    ChiakiEncodedVideoConfig video{}; // header pointer cleared; use header
    ChiakiEncodedAudioConfig audio{};
    std::shared_ptr<const QueuedBuffer> header;
};
class PacketQueue {
public:
    // One instance per source session. Reconnect OBS via attach/detach; a new
    // PS5 session gets a new queue/clock, never reuses raw configuration IDs.
    static constexpr size_t MaxEntries = 256, MaxBytes = 16 * 1024 * 1024;
    static constexpr size_t MaxPacket = 4 * 1024 * 1024, MaxHeader = 64 * 1024;
    static constexpr uint64_t MaxAgeUs = 250000, KeyframeWaitUs = 5000000;
    enum class Result { Accepted, Dropped, Stale, Reset, Invalid };
    enum class Reason { None, Detached, Overflow, StaleMedia, ConfigurationChanged, InvalidData, Timestamp, MissingTrack, KeyframeTimeout };
    enum class MissingStage { None, Configuration, StartupAlignment, Audio, Video, Both };
    struct MissingDetails {
        MissingStage stage=MissingStage::None;
        uint64_t audioAgeUs=0,videoAgeUs=0;
        std::optional<int64_t> audioStartDeltaUs;
    };
    struct Stats { size_t bytes, entries, queued; uint64_t epoch; bool connected, ready; Reason reason; uint64_t oldestReceiptUs=0; MissingDetails missing; };
    explicit PacketQueue(size_t storageLimit = MaxBytes);
    Result configureVideo(const ChiakiEncodedVideoConfig &);
    Result configureAudio(const ChiakiEncodedAudioConfig &);
    bool attach(uint64_t nowUs);
    void detach();
    Result push(MediaTrack, const ChiakiEncodedPacket &, PacketTiming, VideoAccess, uint64_t nowUs);
    std::optional<QueuedPacket> pop(uint64_t nowUs, bool drain = false);
    std::optional<QueueConfiguration> configuration() const;
    Stats stats() const;
    void maintain(uint64_t nowUs);
private:
    mutable std::mutex mutex_;
    std::shared_ptr<QueueBudget> budget_;
    std::array<std::optional<QueuedPacket>, MaxEntries> packets_{};
    QueueConfiguration config_{};
    bool hasVideo_ = false, hasAudio_ = false, connected_ = false, ready_ = false, discardLeading_ = false;
    uint64_t epoch_ = 0, attachedUs_ = 0, startReceiptUs_ = 0;
    uint64_t lastVideoReceiptUs_ = 0, lastAudioReceiptUs_ = 0;
    std::optional<int64_t> startPts_, lastVideoPts_, lastAudioPts_;
    Reason reason_ = Reason::None;
    MissingDetails missing_;
    std::shared_ptr<const QueuedBuffer> copy(const uint8_t *, size_t);
    bool canCopy(size_t size) const;
    void clearPackets();
    void reset(Reason);
    void resetMissing(MissingStage, uint64_t now);
    void maintainLocked(uint64_t);
    void pruneAudio(int64_t boundaryUs);
    void updateReady();
    size_t queued() const;
};
}
