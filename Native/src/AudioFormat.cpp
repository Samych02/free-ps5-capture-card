#include "AudioFormat.h"
#include <stdexcept>
namespace capture {
std::array<uint8_t,19> liveOpusHead(const ChiakiEncodedAudioConfig &config) {
    if(!config.generation || config.channels != 2 || config.sample_rate != 48000 ||
       !config.samples_per_frame || config.samples_per_frame > 5760 || config.samples_per_frame % 120)
        throw std::invalid_argument("Unsupported live Opus configuration");
    return {'O','p','u','s','H','e','a','d',1,2,0,0,0x80,0xbb,0,0,0,0,0};
}
uint32_t opusPacketSamples(const uint8_t *data, size_t size) {
    if(!data || !size || size > 4 * 1024 * 1024) throw std::invalid_argument("Invalid Opus duration header");
    unsigned config = data[0] >> 3;
    uint32_t perFrame;
    if(config < 12) { constexpr uint32_t samples[]{480,960,1920,2880}; perFrame = samples[config & 3]; }
    else if(config < 16) perFrame = (config & 1) ? 960 : 480;
    else perFrame = 120u << (config & 3);
    unsigned code = data[0] & 3;
    unsigned frames = code == 0 ? 1 : 2;
    if(code == 3) {
        if(size < 2) throw std::invalid_argument("Missing Opus frame count");
        frames = data[1] & 63;
    }
    uint32_t total = frames * perFrame;
    if(!frames || total > 5760) throw std::invalid_argument("Unsupported Opus packet duration");
    return total;
}
}
