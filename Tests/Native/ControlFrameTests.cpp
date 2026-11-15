#include "ctrlframe.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

static void header(uint8_t *p,uint32_t n) {
    p[0]=uint8_t(n>>24);p[1]=uint8_t(n>>16);p[2]=uint8_t(n>>8);p[3]=uint8_t(n);
}
int main() {
    std::array<uint8_t,512> buffer{};
    size_t size=999;
    for(size_t received=0;received<8;++received) {
        assert(chiaki_ctrl_frame_size(buffer.data(),received,buffer.size(),&size)==CHIAKI_CTRL_FRAME_INCOMPLETE);
        assert(size==0);
    }
    for(uint32_t payload:{0u,1u,504u}) {
        header(buffer.data(),payload);
        assert(chiaki_ctrl_frame_size(buffer.data(),8+payload,buffer.size(),&size)==CHIAKI_CTRL_FRAME_READY);
        assert(size==8+size_t(payload) && size>=8);
        if(payload)assert(chiaki_ctrl_frame_size(buffer.data(),7+payload,buffer.size(),&size)==CHIAKI_CTRL_FRAME_INCOMPLETE);
    }
    for(uint32_t payload:{505u,0x7fffffffu,0xfffffff7u,0xfffffff8u,0xffffffffu}) {
        header(buffer.data(),payload);
        assert(chiaki_ctrl_frame_size(buffer.data(),8,buffer.size(),&size)==CHIAKI_CTRL_FRAME_INVALID);
        assert(size==0);
    }
    for(uint32_t delta=0;delta<8;++delta) {
        header(buffer.data(),UINT32_MAX-delta);
        assert(chiaki_ctrl_frame_size(buffer.data(),8,buffer.size(),&size)==CHIAKI_CTRL_FRAME_INVALID);
        assert(size==0);
    }
    // Unaligned headers, fragmented payloads, and coalesced frames use the same
    // checked byte count as the real receive loop, never a second expression.
    std::array<uint8_t,513> unaligned{};header(unaligned.data()+1,8);
    assert(chiaki_ctrl_frame_size(unaligned.data()+1,16,512,&size)==CHIAKI_CTRL_FRAME_READY && size==16);
    buffer.fill(0);header(buffer.data(),0);header(buffer.data()+8,4);
    size_t available=20,frames=0;
    while(available>=8) {
        assert(chiaki_ctrl_frame_size(buffer.data(),available,buffer.size(),&size)==CHIAKI_CTRL_FRAME_READY);
        assert(size>=8 && size<=available);available-=size;++frames;
        memmove(buffer.data(),buffer.data()+size,available);
    }
    assert(frames==2 && available==0);
    // Bounded deterministic header fuzzing; every wrap length is tested above.
    uint32_t random=0x12345678;
    for(unsigned i=0;i<100000;++i) {
        random=random*1664525u+1013904223u;header(buffer.data(),random);
        auto result=chiaki_ctrl_frame_size(buffer.data(),buffer.size(),buffer.size(),&size);
        if(random<=504) assert(result==CHIAKI_CTRL_FRAME_READY && size==size_t(random)+8);
        else assert(result==CHIAKI_CTRL_FRAME_INVALID && size==0);
    }
    assert(chiaki_ctrl_frame_size(buffer.data(),513,512,&size)==CHIAKI_CTRL_FRAME_INVALID);
    assert(chiaki_ctrl_frame_size(buffer.data(),0,7,&size)==CHIAKI_CTRL_FRAME_INVALID);
    std::cout << "PASS: control bounds, wrap lengths, unaligned/fragmented/coalesced frames, positive progress, 100000 fuzz headers\n";
}
