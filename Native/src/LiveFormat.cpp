#include "LiveFormat.h"
#include "AnnexB.h"
#include "SPSMetadata.h"
#include <cstring>
#include <stdexcept>
#include <vector>
#include <map>
#include <algorithm>
extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/mastering_display_metadata.h>
}
namespace capture {
namespace {
struct ContextDelete { void operator()(AVCodecContext *p) const { avcodec_free_context(&p); } };
struct ParserDelete { void operator()(AVCodecParserContext *p) const { av_parser_close(p); } };
uint16_t be16(const uint8_t *p) { return uint16_t(p[0]) * 256 + p[1]; }
uint32_t be32(const uint8_t *p) { return uint32_t(be16(p)) * 65536 + be16(p+2); }
void side(AVCodecParameters *p, AVPacketSideDataType type, const void *data, size_t size) {
    if(auto previous = av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,type)) {
        if(previous->size != size || memcmp(previous->data,data,size)) throw std::runtime_error("Conflicting HDR metadata");
        return;
    }
    auto value = av_packet_side_data_new(&p->coded_side_data,&p->nb_coded_side_data,type,size,0);
    if(!value) throw std::bad_alloc();
    memcpy(value->data,data,size);
}
void hevcSEI(AVCodecParameters *parameters, const uint8_t *data, size_t size) {
    auto prefix = [&](size_t i) -> size_t {
        if(size-i>=4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3]==1) return 4;
        if(size-i>=3 && !data[i] && !data[i+1] && data[i+2]==1) return 3;
        return 0;
    };
    for(size_t i=0;i<size;) {
        size_t length=prefix(i); if(!length) { ++i; continue; }
        size_t body=i+length,end=body; while(end<size && !prefix(end)) ++end;
        i=end; if(end-body<2) continue;
        unsigned type=(data[body]>>1)&63; if(type!=39 && type!=40) continue;
        if(end-body>PacketQueue::MaxHeader) throw std::runtime_error("Oversized codec metadata");
        std::vector<uint8_t> rbsp; rbsp.reserve(end-body);
        unsigned zeros=0;
        for(size_t n=body+2;n<end;++n) {
            if(zeros>=2 && data[n]==3) { zeros=0; continue; }
            rbsp.push_back(data[n]); zeros=data[n]==0?zeros+1:0;
        }
        size_t at=0;
        while(at+2<=rbsp.size() && rbsp[at]!=0x80) {
            size_t payloadType=0,payloadSize=0;
            while(at<rbsp.size() && rbsp[at]==255) { payloadType+=255; ++at; }
            if(at==rbsp.size()) break;
            payloadType+=rbsp[at++];
            while(at<rbsp.size() && rbsp[at]==255) { payloadSize+=255; ++at; }
            if(at==rbsp.size()) break;
            payloadSize+=rbsp[at++];
            if(payloadSize>rbsp.size()-at) throw std::runtime_error("Truncated HDR metadata");
            auto p=rbsp.data()+at;
            if(payloadType==137) {
                if(payloadSize!=24) throw std::runtime_error("Invalid mastering display metadata");
                AVMasteringDisplayMetadata md{};
                constexpr int order[]{2,0,1};
                for(int c=0;c<3;++c) for(int xy=0;xy<2;++xy)
                    md.display_primaries[c][xy]={be16(p+order[c]*4+xy*2),50000};
                md.white_point[0]={be16(p+12),50000}; md.white_point[1]={be16(p+14),50000};
                md.max_luminance={int(be32(p+16)),10000}; md.min_luminance={int(be32(p+20)),10000};
                md.has_primaries=1; md.has_luminance=1;
                side(parameters,AV_PKT_DATA_MASTERING_DISPLAY_METADATA,&md,sizeof(md));
            } else if(payloadType==144) {
                if(payloadSize!=4) throw std::runtime_error("Invalid content light metadata");
                AVContentLightMetadata light{}; light.MaxCLL=be16(p); light.MaxFALL=be16(p+2);
                side(parameters,AV_PKT_DATA_CONTENT_LIGHT_LEVEL,&light,sizeof(light));
            }
            at+=payloadSize;
        }
    }
}
void refreshHDRExtradata(AVCodecParameters *p) {
    if(p->codec_id!=AV_CODEC_ID_HEVC || !p->extradata_size)return;
    if(!p->extradata || p->extradata_size<0 || size_t(p->extradata_size)>PacketQueue::MaxHeader)
        throw std::runtime_error("Invalid HEVC codec-private header");
    const auto *data=p->extradata;size_t size=p->extradata_size;
    auto prefix=[&](size_t i)->size_t {
        if(size-i>=4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3]==1)return 4;
        if(size-i>=3 && !data[i] && !data[i+1] && data[i+2]==1)return 3;return 0;
    };
    std::vector<uint8_t> header;
    for(size_t i=0;i<size;) {
        auto n=prefix(i);if(!n){++i;continue;}
        auto body=i+n,end=body;while(end<size && !prefix(end))++end;
        if(end-body<2)throw std::runtime_error("Truncated HEVC codec-private NAL");
        auto type=(data[body]>>1)&63;
        // Codec-private HDR state must agree with the latest container metadata.
        // Source access units (including every original SEI) are untouched.
        bool hdrSEI=false;
        if(type==39 || type==40) {
            Parameters metadata(avcodec_parameters_alloc());if(!metadata)throw std::bad_alloc();
            hevcSEI(metadata.get(),data+i,end-i);hdrSEI=metadata->nb_coded_side_data!=0;
        }
        if(!hdrSEI)header.insert(header.end(),data+i,data+end);
        i=end;
    }
    if(header.empty())throw std::runtime_error("HEVC codec-private header is not Annex-B");
    auto emit=[&](unsigned type,const std::vector<uint8_t> &payload) {
        std::vector<uint8_t> rbsp{uint8_t(type),uint8_t(payload.size())};
        rbsp.insert(rbsp.end(),payload.begin(),payload.end());rbsp.push_back(0x80);
        header.insert(header.end(),{0,0,0,1,0x4e,1});unsigned zeros=0;
        for(auto byte:rbsp) {
            if(zeros>=2 && byte<=3) {header.push_back(3);zeros=0;}
            header.push_back(byte);zeros=byte==0?zeros+1:0;
        }
    };
    auto put=[&](std::vector<uint8_t> &payload,uint64_t value,unsigned bytes) {
        if(value>(bytes==2?UINT16_MAX:UINT32_MAX))throw std::runtime_error("Out-of-range HDR metadata");
        while(bytes--)payload.push_back(uint8_t(value>>(bytes*8)));
    };
    auto scaled=[](AVRational value,uint64_t scale)->uint64_t {
        if(value.num<0 || value.den<=0)throw std::runtime_error("Invalid HDR metadata rational");
        return (uint64_t(value.num)*scale+unsigned(value.den)/2)/unsigned(value.den);
    };
    if(auto side=av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,AV_PKT_DATA_MASTERING_DISPLAY_METADATA)) {
        if(side->size!=sizeof(AVMasteringDisplayMetadata))throw std::runtime_error("Invalid mastering display metadata");
        const auto &md=*reinterpret_cast<const AVMasteringDisplayMetadata *>(side->data);
        std::vector<uint8_t> payload;
        for(unsigned colour:{1u,2u,0u})for(unsigned xy=0;xy<2;++xy)put(payload,scaled(md.display_primaries[colour][xy],50000),2);
        for(unsigned xy=0;xy<2;++xy)put(payload,scaled(md.white_point[xy],50000),2);
        put(payload,scaled(md.max_luminance,10000),4);put(payload,scaled(md.min_luminance,10000),4);emit(137,payload);
    }
    if(auto side=av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,AV_PKT_DATA_CONTENT_LIGHT_LEVEL)) {
        if(side->size!=sizeof(AVContentLightMetadata))throw std::runtime_error("Invalid content light metadata");
        const auto &md=*reinterpret_cast<const AVContentLightMetadata *>(side->data);
        std::vector<uint8_t> payload;put(payload,md.MaxCLL,2);put(payload,md.MaxFALL,2);emit(144,payload);
    }
    if(header.size()>PacketQueue::MaxHeader)throw std::runtime_error("Oversized updated HEVC header");
    auto copy=static_cast<uint8_t *>(av_mallocz(header.size()+AV_INPUT_BUFFER_PADDING_SIZE));if(!copy)throw std::bad_alloc();
    memcpy(copy,header.data(),header.size());av_freep(&p->extradata);p->extradata=copy;p->extradata_size=int(header.size());
}
}
Parameters readHDRMetadata(const uint8_t *data,size_t size) {
    if(!data || !size || size>PacketQueue::MaxPacket) throw std::runtime_error("Invalid HDR metadata buffer");
    Parameters result(avcodec_parameters_alloc());if(!result) throw std::bad_alloc();
    hevcSEI(result.get(),data,size);
    if(!result->nb_coded_side_data) return {};
    return result;
}
bool applyHDRMetadata(AVCodecParameters *target,const AVCodecParameters *updates) {
    if(!target) throw std::runtime_error("Missing metadata destination");
    if(!updates) return false;
    bool changed=false;
    for(auto type:{AV_PKT_DATA_MASTERING_DISPLAY_METADATA,AV_PKT_DATA_CONTENT_LIGHT_LEVEL}) {
        auto value=av_packet_side_data_get(updates->coded_side_data,updates->nb_coded_side_data,type);
        if(!value) continue;
        size_t expected=type==AV_PKT_DATA_CONTENT_LIGHT_LEVEL?sizeof(AVContentLightMetadata):sizeof(AVMasteringDisplayMetadata);
        if(value->size!=expected) throw std::runtime_error("Invalid static HDR metadata size");
        auto old=av_packet_side_data_get(target->coded_side_data,target->nb_coded_side_data,type);
        if(old && old->size==value->size && !memcmp(old->data,value->data,value->size)) continue;
        av_packet_side_data_remove(target->coded_side_data,&target->nb_coded_side_data,type);
        side(target,type,value->data,value->size);changed=true;
    }
    if(changed)refreshHDRExtradata(target);
    return changed;
}
std::optional<std::vector<uint8_t>> updatedVideoHeader(ChiakiCodec codec,const uint8_t *header,size_t headerSize,const uint8_t *packet,size_t packetSize) {
    if(!header || !headerSize || headerSize>PacketQueue::MaxHeader || !packet || !packetSize || packetSize>PacketQueue::MaxPacket)
        throw std::runtime_error("Invalid video header update");
    using Key=std::pair<unsigned,uint32_t>;
    std::map<Key,std::vector<uint8_t>> sets;
    std::vector<std::vector<uint8_t>> extras;
    size_t total=0;bool changed=false;
    auto collect=[&](const uint8_t *data,size_t size,bool baseline) {
        std::map<Key,std::pair<const uint8_t *,size_t>> seen;
        auto prefix=[&](size_t i)->size_t {
            if(size-i>=4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3]==1)return 4;
            if(size-i>=3 && !data[i] && !data[i+1] && data[i+2]==1)return 3;return 0;
        };
        unsigned count=0;
        for(size_t i=0;i<size;) {
            auto n=prefix(i);if(!n){++i;continue;}
            auto body=i+n,end=body;while(end<size && !prefix(end))++end;i=end;
            while(end>body && !data[end-1])--end;
            if(body==end || ++count>1024)throw std::runtime_error("Malformed parameter-set update");
            unsigned type=codec==CHIAKI_CODEC_H264?(data[body]&31):((data[body]>>1)&63);
            bool parameter=codec==CHIAKI_CODEC_H264?(type==7 || type==8):(type>=32 && type<=34);
            if(!parameter) {
                if(baseline) {if(total+4+end-body>PacketQueue::MaxHeader)throw std::runtime_error("Oversized header");extras.emplace_back(data+body,data+end);total+=4+end-body;}
                continue;
            }
            Key key{type,parameterSetID(codec,data+body,end-body)};
            auto prior=seen.find(key);
            if(prior!=seen.end() && (prior->second.second!=end-body || memcmp(prior->second.first,data+body,end-body)))
                throw std::runtime_error("Conflicting parameter-set IDs");
            seen[key]={data+body,end-body};
            auto old=sets.find(key);
            if(old!=sets.end() && old->second.size()==end-body && !memcmp(old->second.data(),data+body,end-body))continue;
            if(baseline && old!=sets.end())throw std::runtime_error("Conflicting header parameter sets");
            size_t previous=old==sets.end()?0:4+old->second.size();
            if(total-previous+4+end-body>PacketQueue::MaxHeader)throw std::runtime_error("Oversized header update");
            total=total-previous+4+end-body;sets[key]=std::vector<uint8_t>(data+body,data+end);
            if(!baseline)changed=true;
        }
    };
    collect(header,headerSize,true);collect(packet,packetSize,false);
    if(!changed)return {};
    for(unsigned type:codec==CHIAKI_CODEC_H264?std::vector<unsigned>{7,8}:std::vector<unsigned>{32,33,34}) {
        if(std::none_of(sets.begin(),sets.end(),[&](const auto &value){return value.first.first==type;}))
            throw std::runtime_error("Incomplete video parameter sets");
    }
    std::vector<uint8_t> result;result.reserve(total);
    auto append=[&](const auto &bytes){result.insert(result.end(),{0,0,0,1});result.insert(result.end(),bytes.begin(),bytes.end());};
    for(const auto &value:sets)append(value.second);
    for(const auto &value:extras)append(value);
    return result;
}
bool hdrMetadataMatches(const AVCodecParameters *baseline,const uint8_t *data,size_t size) {
    if(!baseline || !data || size>PacketQueue::MaxPacket) throw std::runtime_error("Invalid HDR metadata buffer");
    Parameters current(avcodec_parameters_alloc()); if(!current) throw std::bad_alloc();
    hevcSEI(current.get(),data,size);
    for(int i=0;i<current->nb_coded_side_data;++i) {
        const auto &value=current->coded_side_data[i];
        auto old=av_packet_side_data_get(baseline->coded_side_data,baseline->nb_coded_side_data,value.type);
        if(!old || old->size!=value.size || memcmp(old->data,value.data,value.size)) return false;
    }
    return true;
}
VideoFormat parseVideoFormat(ChiakiCodec codec, const uint8_t *header, size_t headerSize, const uint8_t *frame, size_t frameSize) {
    if(!header || !headerSize || headerSize>PacketQueue::MaxHeader || !frame || !frameSize || frameSize>PacketQueue::MaxPacket)
        throw std::runtime_error("Missing or oversized video configuration");
    auto info=inspectAccessUnit(codec,frame,frameSize);
    if(!info.hasPicture || (info.access!=VideoAccess::IDR && info.access!=VideoAccess::CRA)) throw std::runtime_error("Need random-access video for configuration");
    AVCodecID id=codec==CHIAKI_CODEC_H264?AV_CODEC_ID_H264:AV_CODEC_ID_HEVC;
    std::unique_ptr<AVCodecContext,ContextDelete> context(avcodec_alloc_context3(nullptr));
    std::unique_ptr<AVCodecParserContext,ParserDelete> parser(av_parser_init(id));
    if(!context || !parser) throw std::runtime_error("Codec parser unavailable");
    context->codec_id=id; context->codec_type=AVMEDIA_TYPE_VIDEO;
    context->extradata=static_cast<uint8_t *>(av_mallocz(headerSize+AV_INPUT_BUFFER_PADDING_SIZE));
    if(!context->extradata) throw std::bad_alloc();
    context->extradata_size=int(headerSize); memcpy(context->extradata,header,headerSize);
    std::vector<uint8_t> input(headerSize+frameSize+AV_INPUT_BUFFER_PADDING_SIZE,0);
    memcpy(input.data(),header,headerSize); memcpy(input.data()+headerSize,frame,frameSize);
    parser->flags|=PARSER_FLAG_COMPLETE_FRAMES;
    uint8_t *output=nullptr; int outputSize=0;
    if(av_parser_parse2(parser.get(),context.get(),&output,&outputSize,input.data(),int(headerSize+frameSize),AV_NOPTS_VALUE,AV_NOPTS_VALUE,0)<0 || !outputSize)
        throw std::runtime_error("Cannot parse video configuration");
    if(context->has_b_frames) throw std::runtime_error("Presentation reordering is not supported by this source timeline");
    if(context->pix_fmt==AV_PIX_FMT_NONE && parser->format>=0) context->pix_fmt=static_cast<AVPixelFormat>(parser->format);
    auto descriptor=av_pix_fmt_desc_get(context->pix_fmt);
    auto rate=context->framerate;
    if(!descriptor || rate.num<=0 || rate.den<=0 || av_q2d(rate)>60.01 || !context->width || !context->height || context->width>1920 || context->height>1080)
        throw std::runtime_error("Unverified codec cadence or unsupported dimensions");
    VideoFormat result; result.parameters.reset(avcodec_parameters_alloc());
    if(!result.parameters || avcodec_parameters_from_context(result.parameters.get(),context.get())<0) throw std::bad_alloc();
    result.frameRate=rate; result.depth=descriptor->comp[0].depth;
    readSPSColour(codec,header,headerSize,result.parameters.get());
    result.hdr=result.depth==10 && result.parameters->color_primaries==AVCOL_PRI_BT2020 && result.parameters->color_trc==AVCOL_TRC_SMPTE2084 && result.parameters->color_space==AVCOL_SPC_BT2020_NCL;
    if((codec==CHIAKI_CODEC_H265_HDR)!=result.hdr) throw std::runtime_error("Requested and signaled HDR modes do not match");
    if(id==AV_CODEC_ID_HEVC) {
        auto fromHeader=readHDRMetadata(header,headerSize),fromFrame=readHDRMetadata(frame,frameSize);
        applyHDRMetadata(result.parameters.get(),fromHeader.get());applyHDRMetadata(result.parameters.get(),fromFrame.get());
    }
    return result;
}
Parameters liveAudioParameters(const ChiakiEncodedAudioConfig &config) {
    auto header=liveOpusHead(config); Parameters p(avcodec_parameters_alloc()); if(!p) throw std::bad_alloc();
    p->codec_type=AVMEDIA_TYPE_AUDIO; p->codec_id=AV_CODEC_ID_OPUS; p->sample_rate=48000;
    av_channel_layout_default(&p->ch_layout,2); p->initial_padding=0; p->seek_preroll=3840;
    p->extradata=static_cast<uint8_t *>(av_mallocz(header.size()+AV_INPUT_BUFFER_PADDING_SIZE));
    if(!p->extradata) throw std::bad_alloc();
    p->extradata_size=int(header.size()); memcpy(p->extradata,header.data(),header.size()); return p;
}
}
