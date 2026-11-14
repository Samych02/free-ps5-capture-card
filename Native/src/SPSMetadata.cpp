#include "SPSMetadata.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
namespace capture {
namespace {
class Bits {
    std::vector<uint8_t> bytes_; size_t pos_=0;
public:
    Bits(const uint8_t *p,size_t n) {
        unsigned zeros=0;
        for(size_t i=0;i<n;++i) {
            if(zeros>=2 && p[i]==3) { zeros=0; continue; }
            bytes_.push_back(p[i]); zeros=p[i]==0?zeros+1:0;
        }
    }
    uint32_t u(unsigned n) {
        if(n>32 || n>bytes_.size()*8-pos_) throw std::runtime_error("Truncated SPS");
        uint32_t v=0; while(n--) { v=(v<<1)|((bytes_[pos_/8]>>(7-pos_%8))&1); ++pos_; } return v;
    }
    void skip(unsigned n) { while(n) { unsigned k=std::min(n,32u); u(k); n-=k; } }
    uint32_t ue() { unsigned n=0; while(!u(1)) if(++n>31) throw std::runtime_error("Oversized SPS field"); return uint32_t(((uint64_t(1)<<n)-1)+u(n)); }
    int64_t se() { auto v=ue(); return (v&1)?int64_t((uint64_t(v)+1)/2):-int64_t(v/2); }
    uint32_t bounded(unsigned max) { auto n=ue(); if(n>max) throw std::runtime_error("Unsupported SPS field"); return n; }
};
struct Colour { int range=AVCOL_RANGE_UNSPECIFIED,primaries=AVCOL_PRI_UNSPECIFIED,trc=AVCOL_TRC_UNSPECIFIED,space=AVCOL_SPC_UNSPECIFIED; };
Colour vui(Bits &b) {
    Colour c;
    if(b.u(1)) { if(b.u(8)==255) b.skip(32); }
    if(b.u(1)) b.skip(1);
    if(b.u(1)) {
        b.skip(3); c.range=b.u(1)?AVCOL_RANGE_JPEG:AVCOL_RANGE_MPEG;
        if(b.u(1)) { c.primaries=b.u(8); c.trc=b.u(8); c.space=b.u(8); }
    }
    return c;
}
Colour h264(Bits &b) {
    unsigned profile=b.u(8); b.skip(16); b.ue();
    if(profile==100 || profile==110 || profile==122 || profile==244 || profile==44 || profile==83 || profile==86 ||
       profile==118 || profile==128 || profile==138 || profile==139 || profile==134 || profile==135) {
        unsigned chroma=b.bounded(3); if(chroma==3) b.skip(1);
        b.bounded(6); b.bounded(6); b.skip(1);
        if(b.u(1)) for(unsigned i=0;i<(chroma==3?12u:8u);++i) if(b.u(1)) {
            int64_t last=8,next=8;
            for(unsigned j=0;j<(i<6?16u:64u);++j) { if(next) next=(last+b.se()+256)%256; if(next) last=next; }
        }
    }
    b.bounded(12); unsigned poc=b.bounded(2);
    if(!poc) b.bounded(12);
    else if(poc==1) { b.skip(1); b.se(); b.se(); auto n=b.bounded(256); while(n--) b.se(); }
    b.ue(); b.skip(1); b.ue(); b.ue();
    if(!b.u(1)) throw std::runtime_error("Interlaced source timing is unsupported");
    b.skip(1); if(b.u(1)) for(int i=0;i<4;++i) b.ue();
    if(!b.u(1)) {
        if(profile!=66) throw std::runtime_error("Unverified H264 reorder constraints");
        return {};
    }
    Colour colour=vui(b);
    if(b.u(1)) { b.ue(); b.ue(); }
    if(b.u(1)) b.skip(65);
    auto hrd=[&] {
        unsigned count=b.bounded(31); b.skip(8);
        for(unsigned i=0;i<=count;++i) { b.ue(); b.ue(); b.skip(1); }
        b.skip(20);
    };
    bool nal=b.u(1); if(nal) hrd(); bool vcl=b.u(1); if(vcl) hrd();
    if(nal || vcl) b.skip(1);
    b.skip(1);
    if(b.u(1)) {
        b.skip(1); for(int i=0;i<4;++i) b.ue();
        if(b.ue()) throw std::runtime_error("H264 presentation reordering is unsupported");
        b.ue();
    } else if(profile!=66) throw std::runtime_error("Unverified H264 reorder constraints");
    return colour;
}
Colour hevc(Bits &b) {
    b.skip(4); unsigned layers=b.u(3); b.skip(1); b.skip(96);
    bool profile[7]{},level[7]{};
    for(unsigned i=0;i<layers;++i) { profile[i]=b.u(1); level[i]=b.u(1); }
    if(layers) b.skip((8-layers)*2);
    for(unsigned i=0;i<layers;++i) { if(profile[i]) b.skip(88); if(level[i]) b.skip(8); }
    b.ue(); if(b.bounded(3)==3) b.skip(1);
    b.ue(); b.ue(); if(b.u(1)) for(int i=0;i<4;++i) b.ue();
    b.bounded(8); b.bounded(8); unsigned pocBits=b.bounded(28)+4;
    bool orderAll=b.u(1);
    for(unsigned i=orderAll?0:layers;i<=layers;++i) {
        b.ue(); if(b.ue()) throw std::runtime_error("Reordered video is unsupported"); b.ue();
    }
    for(int i=0;i<6;++i) b.ue();
    if(b.u(1) && b.u(1)) {
        for(unsigned size=0;size<4;++size) for(unsigned matrix=0;matrix<6;matrix+=size==3?3:1) {
            if(!b.u(1)) b.ue();
            else { if(size>1) b.se(); for(unsigned i=0;i<std::min(64u,1u<<(4+(size<<1)));++i) b.se(); }
        }
    }
    b.skip(2);
    if(b.u(1)) { b.skip(8); b.ue(); b.ue(); b.skip(1); }
    unsigned sets=b.bounded(64); unsigned delta[64]{};
    for(unsigned i=0;i<sets;++i) {
        if(i && b.u(1)) {
            b.skip(1); b.ue();
            for(unsigned j=0;j<=delta[i-1];++j) { bool used=b.u(1); bool useDelta=used?true:b.u(1); if(useDelta) ++delta[i]; }
        } else {
            unsigned negative=b.bounded(32),positive=b.bounded(32); delta[i]=negative+positive;
            for(unsigned j=0;j<delta[i];++j) { b.ue(); b.skip(1); }
        }
        if(delta[i]>64) throw std::runtime_error("Oversized SPS reference set");
    }
    if(b.u(1)) { auto n=b.bounded(32); while(n--) b.skip(pocBits+1); }
    b.skip(2); return b.u(1)?vui(b):Colour{};
}
}
uint32_t parameterSetID(ChiakiCodec codec,const uint8_t *data,size_t size) {
    bool avc=codec==CHIAKI_CODEC_H264;size_t header=avc?1:2;
    if(!data || size<=header || size>65536) throw std::runtime_error("Invalid parameter set");
    unsigned type=avc?(data[0]&31):((data[0]>>1)&63);
    Bits bits(data+header,size-header);
    if(avc) {
        if(type==7) {bits.skip(24);return bits.bounded(31);}
        if(type==8) return bits.bounded(255);
    } else {
        if(type==32) return bits.u(4);
        if(type==34) return bits.bounded(63);
        if(type==33) {
            bits.skip(4);unsigned layers=bits.u(3);bits.skip(1);bits.skip(96);
            bool profile[7]{},level[7]{};
            for(unsigned i=0;i<layers;++i) {profile[i]=bits.u(1);level[i]=bits.u(1);}
            if(layers) bits.skip((8-layers)*2);
            for(unsigned i=0;i<layers;++i) {if(profile[i])bits.skip(88);if(level[i])bits.skip(8);}
            return bits.bounded(15);
        }
    }
    throw std::runtime_error("Unsupported parameter-set type");
}
void readSPSColour(ChiakiCodec codec,const uint8_t *data,size_t size,AVCodecParameters *p) {
    if(!data || size>65536) throw std::runtime_error("Oversized SPS configuration");
    auto prefix=[&](size_t i)->size_t {
        if(size-i>=4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3]==1) return 4;
        if(size-i>=3 && !data[i] && !data[i+1] && data[i+2]==1) return 3; return 0;
    };
    bool found=false; Colour previous;
    for(size_t i=0;i<size;) {
        auto n=prefix(i); if(!n) { ++i; continue; }
        size_t body=i+n,end=body; while(end<size && !prefix(end)) ++end; i=end;
        if(body==end) continue;
        bool avc=codec==CHIAKI_CODEC_H264;
        if((avc?(data[body]&31):((data[body]>>1)&63))!=(avc?7:33)) continue;
        size_t header=avc?1:2; if(end-body<=header) throw std::runtime_error("Truncated SPS header");
        Bits bits(data+body+header,end-body-header); auto c=avc?h264(bits):hevc(bits);
        if(found && (c.range!=previous.range || c.primaries!=previous.primaries || c.trc!=previous.trc || c.space!=previous.space))
            throw std::runtime_error("Conflicting SPS colour metadata");
        found=true; previous=c;
    }
    if(!found) throw std::runtime_error("Missing SPS configuration");
    p->color_range=static_cast<AVColorRange>(previous.range); p->color_primaries=static_cast<AVColorPrimaries>(previous.primaries);
    p->color_trc=static_cast<AVColorTransferCharacteristic>(previous.trc); p->color_space=static_cast<AVColorSpace>(previous.space);
}
}
