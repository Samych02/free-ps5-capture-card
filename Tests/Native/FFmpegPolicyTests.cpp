#include <iostream>
#include <set>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main() {
    void *opaque=nullptr;
    size_t codecs=0;
    while(av_codec_iterate(&opaque)) ++codecs;
    opaque=nullptr;
    std::set<int> parsers;
    while(auto parser=av_parser_iterate(&opaque))
        for(auto id:parser->codec_ids) if(id) parsers.insert(id);
    opaque=nullptr;
    std::set<std::string> muxers;
    while(auto muxer=av_muxer_iterate(&opaque)) muxers.insert(muxer->name);
    opaque=nullptr;
    size_t demuxers=0;
    while(av_demuxer_iterate(&opaque)) ++demuxers;
    size_t protocols=0;
    for(int output:{0,1}) {
        opaque=nullptr;
        while(avio_enum_protocols(&opaque,output)) ++protocols;
    }
    bool allowed=codecs==0 && parsers==std::set<int>{AV_CODEC_ID_H264,AV_CODEC_ID_HEVC}
        && muxers==std::set<std::string>{"matroska"} && demuxers==0 && protocols==0;
    std::cout << "FFmpeg runtime: codecs=" << codecs << ", parsers=" << parsers.size()
        << ", muxers=" << muxers.size() << ", demuxers=" << demuxers
        << ", protocols=" << protocols << '\n';
    std::cout << "FFmpeg license: " << avcodec_license() << '\n';
    if(!allowed) std::cerr << "Runtime must contain only H264/HEVC parsers and Matroska output; no codecs or input/network protocols\n";
    return allowed?0:1;
}
