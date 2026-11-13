#include "EncodedMedia.h"
namespace capture {
ChiakiEncodedMediaSink EncodedMedia::sink() {
    return {this,
        [](const ChiakiEncodedVideoConfig *config, void *ctx) {
            auto &self = *static_cast<EncodedMedia *>(ctx);
            EncodedEvent event{EncodedEvent::Kind::VideoConfig};
            event.video = *config; event.video.header = nullptr;
            self.emit(std::move(event), config->header, config->header_size, MaxHeader);
        },
        [](const ChiakiEncodedAudioConfig *config, void *ctx) {
            auto &self = *static_cast<EncodedMedia *>(ctx);
            EncodedEvent event{EncodedEvent::Kind::AudioConfig}; event.audio = *config;
            self.emit(std::move(event));
        },
        [](const ChiakiEncodedPacket *packet, void *ctx) {
            auto &self = *static_cast<EncodedMedia *>(ctx);
            EncodedEvent event{EncodedEvent::Kind::Video}; event.packet = *packet; event.packet.data = nullptr;
            return self.emit(std::move(event), packet->data, packet->size, MaxPacket);
        },
        [](const ChiakiEncodedPacket *packet, void *ctx) {
            auto &self = *static_cast<EncodedMedia *>(ctx);
            EncodedEvent event{EncodedEvent::Kind::Audio}; event.packet = *packet; event.packet.data = nullptr;
            self.emit(std::move(event), packet->data, packet->size, MaxPacket);
        }};
}
bool EncodedMedia::emit(EncodedEvent event, const uint8_t *data, size_t size, size_t limit) noexcept {
    if(failed_.load()) return false;
    if(size > limit || (size && !data)) { failed_ = true; return false; }
    if(!consumer_) return true;
    try {
        if(size) event.bytes.assign(data, data + size);
        return consumer_(std::move(event));
    } catch(...) { failed_ = true; return false; }
}
ChiakiErrorCode SessionMediaControl::requestKeyframe() {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_ ? chiaki_session_request_idr(session_) : CHIAKI_ERR_CANCELED;
}
ChiakiSession *SessionMediaControl::stopAndDetach() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto *session = session_; session_ = nullptr;
    if(session) stop_(session);
    return session;
}
}
