#include "AnnexB.h"
#include <stdexcept>
#include <vector>
#include <algorithm>
namespace capture {
uint64_t parameterSetSignature(ChiakiCodec codec,const uint8_t *data,size_t size) {
    if(!data || !size || size>PacketQueue::MaxPacket) throw std::invalid_argument("Invalid parameter-set buffer");
    auto prefix=[&](size_t i)->size_t {
        if(size-i>=4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3]==1) return 4;
        if(size-i>=3 && !data[i] && !data[i+1] && data[i+2]==1) return 3; return 0;
    };
    uint64_t hash=1469598103934665603ULL;
    std::vector<std::pair<const uint8_t *,size_t>> sets;
    for(size_t i=0;i<size;) {
        auto n=prefix(i); if(!n) { ++i; continue; }
        size_t body=i+n,end=body; while(end<size && !prefix(end)) ++end; i=end;
        if(body==end) continue;
        unsigned type=codec==CHIAKI_CODEC_H264?(data[body]&31):((data[body]>>1)&63);
        bool parameter=codec==CHIAKI_CODEC_H264?(type==7 || type==8):(type>=32 && type<=34);
        if(!parameter) continue;
        while(end>body && !data[end-1]) --end;
        if(sets.size()>=1024) throw std::invalid_argument("Too many parameter sets");
        sets.emplace_back(data+body,end-body);
    }
    auto less=[](auto a,auto b){return std::lexicographical_compare(a.first,a.first+a.second,b.first,b.first+b.second);};
    std::sort(sets.begin(),sets.end(),less);
    sets.erase(std::unique(sets.begin(),sets.end(),[&](auto a,auto b){return !less(a,b) && !less(b,a);}),sets.end());
    for(auto set:sets) {
        for(size_t j=0;j<set.second;++j) hash=(hash^set.first[j])*1099511628211ULL;
        hash=(hash^uint64_t(set.second))*1099511628211ULL;
    }
    return sets.empty()?0:hash;
}
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
