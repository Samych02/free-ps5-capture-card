#pragma once
#include <chiaki/encodedmedia.h>
#include <array>
namespace capture {
// Live PS5 baseline, no fixture-encoder lookahead/pre-skip or invented resampling.
std::array<uint8_t,19> liveOpusHead(const ChiakiEncodedAudioConfig &);
// RFC 6716 TOC duration, not a decoder or full compressed-payload validator.
uint32_t opusPacketSamples(const uint8_t *, size_t);
}
