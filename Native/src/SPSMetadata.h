#pragma once
#include "LiveFormat.h"
namespace capture {
uint32_t parameterSetID(ChiakiCodec,const uint8_t *,size_t);
void readSPSColour(ChiakiCodec, const uint8_t *, size_t, AVCodecParameters *);
}
