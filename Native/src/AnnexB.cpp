#include "AnnexB.h"
#include <stdexcept>
namespace capture {
AccessUnitInfo inspectAccessUnit(ChiakiCodec codec, const uint8_t *data, size_t size) {
    auto invalid = [] { throw std::invalid_argument("Malformed or unsupported Annex-B access unit"); };
    if(!data || !size || size > PacketQueue::MaxPacket ||
       (codec != CHIAKI_CODEC_H264 && codec != CHIAKI_CODEC_H265 && codec != CHIAKI_CODEC_H265_HDR)) invalid();
    auto prefix = [&](size_t i) -> size_t {
        if(size - i >= 4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3] == 1) return 4;
        if(size - i >= 3 && !data[i] && !data[i+1] && data[i+2] == 1) return 3;
        return 0;
    };
    auto find = [&](size_t from) {
        for(size_t i = from; i < size; ++i) if(prefix(i)) return i;
        return size;
    };
    size_t start = find(0), count = 0;
    if(start == size) invalid();
    for(size_t i = 0; i < start; ++i) if(data[i]) invalid();
    AccessUnitInfo info;
    while(start < size) {
        if(++count > 1024) invalid();
        size_t body = start + prefix(start), end = find(body);
        if(body == end || (data[body] & 0x80)) invalid();
        bool picture;
        VideoAccess access = VideoAccess::Predicted;
        if(codec == CHIAKI_CODEC_H264) {
            unsigned type = data[body] & 31;
            if(!type || type >= 24) invalid();
            picture = type <= 5;
            if(picture && end - body < 2) invalid();
            if(type == 5) access = VideoAccess::IDR;
        } else {
            if(end - body < 2 || !(data[body+1] & 7)) invalid();
            unsigned type = (data[body] >> 1) & 63;
            if((type >= 10 && type <= 15) || (type >= 22 && type <= 31)) invalid();
            picture = type <= 31;
            if(picture && end - body < 3) invalid();
            if(type == 21) access = VideoAccess::CRA;
            else if(type >= 16 && type <= 20) access = VideoAccess::IDR;
            else if(type >= 6 && type <= 9) access = VideoAccess::Leading;
        }
        if(picture) {
            if(info.hasPicture && info.access != access) invalid();
            info.hasPicture = true; info.access = access;
        }
        start = end;
    }
    return info;
}
}
