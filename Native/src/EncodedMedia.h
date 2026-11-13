#pragma once
#include <chiaki/session.h>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>

namespace capture {
struct EncodedEvent {
    enum class Kind { VideoConfig, AudioConfig, Video, Audio } kind;
    ChiakiEncodedVideoConfig video{};
    ChiakiEncodedAudioConfig audio{};
    ChiakiEncodedPacket packet{};
    // For MISSING audio, first_receipt_us is the arrival that exposed the hole,
    // not a receipt time for a payload that never arrived; use samples/index.
    // All borrowed pointers in the structs above are cleared after copying.
    std::vector<uint8_t> bytes;
};
class EncodedMedia {
public:
    static constexpr size_t MaxHeader = 64 * 1024, MaxPacket = 4 * 1024 * 1024;
    // Runs on receive threads. The consumer must enqueue/discard without I/O or
    // unbounded work; it receives owned bytes. No consumer is discard+accept.
    using Consumer = std::function<bool(EncodedEvent)>;
    explicit EncodedMedia(Consumer consumer = {}) : consumer_(std::move(consumer)) {}
    ChiakiEncodedMediaSink sink();
    bool failed() const { return failed_.load(); }
private:
    Consumer consumer_;
    std::atomic<bool> failed_{false};
    bool emit(EncodedEvent event, const uint8_t *data = nullptr, size_t size = 0, size_t limit = 0) noexcept;
};

// Lifecycle owner closes this gate BEFORE joining/finalizing a session. Requests
// and stop/detach are serialized; join/fini happen outside the lock/callbacks.
class SessionMediaControl {
public:
    explicit SessionMediaControl(ChiakiSession *session,decltype(&chiaki_session_stop) stop=chiaki_session_stop) : session_(session),stop_(stop) {}
    ChiakiErrorCode requestKeyframe();
    ChiakiSession *stopAndDetach();
private:
    std::mutex mutex_;
    ChiakiSession *session_;
    decltype(&chiaki_session_stop) stop_;
};
}
