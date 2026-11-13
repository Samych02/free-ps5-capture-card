#pragma once
#include "PacketQueue.h"
#include <cstddef>
#include <cstdint>
namespace capture {
struct AccessUnitInfo { bool hasPicture = false; VideoAccess access = VideoAccess::Predicted; };
// Bounded NAL-header classification only, not a slice/colour/cadence parser.
// A non-IDR H264 I-slice is not random access. HEVC CRA and leading pictures
// remain distinct so consumers can suppress associated RASL/RADL pictures.
AccessUnitInfo inspectAccessUnit(ChiakiCodec, const uint8_t *, size_t);
}
