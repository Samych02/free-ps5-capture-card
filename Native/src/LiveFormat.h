#pragma once
#include "AudioFormat.h"
#include <memory>
extern "C" {
#include <libavcodec/avcodec.h>
}
namespace capture {
struct ParametersDeleter { void operator()(AVCodecParameters *p) const { avcodec_parameters_free(&p); } };
using Parameters = std::unique_ptr<AVCodecParameters, ParametersDeleter>;
struct VideoFormat { Parameters parameters; AVRational frameRate{}; int depth = 0; bool hdr = false; };
VideoFormat parseVideoFormat(ChiakiCodec, const uint8_t *header, size_t headerSize, const uint8_t *frame, size_t frameSize);
Parameters liveAudioParameters(const ChiakiEncodedAudioConfig &);
bool hdrMetadataMatches(const AVCodecParameters *,const uint8_t *,size_t);
}
