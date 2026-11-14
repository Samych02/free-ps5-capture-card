#include "Pipeline.h"
#include "AnnexB.h"
#include <chiaki/time.h>
#include <csignal>
#include <iostream>
#include <poll.h>
extern "C" {
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
}
static volatile sig_atomic_t stopped=0;
static void stop(int) { stopped=1; }
static std::vector<uint8_t> withoutHEVCSEI(const uint8_t *data,size_t size) {
    auto prefix=[&](size_t i)->size_t {
        if(size-i>=4 && !data[i] && !data[i+1] && !data[i+2] && data[i+3]==1)return 4;
        if(size-i>=3 && !data[i] && !data[i+1] && data[i+2]==1)return 3;
        return 0;
    };
    std::vector<uint8_t> output;
    for(size_t start=0;start<size;) {
        auto length=prefix(start);if(!length){++start;continue;}
        auto body=start+length,end=body;while(end<size && !prefix(end))++end;
        unsigned type=body<end?(data[body]>>1)&63:0;
        if(type!=39 && type!=40)output.insert(output.end(),data+start,data+end);
        start=end;
    }
    return output;
}
int main(int argc,char **argv) {
    AVFormatContext *input=nullptr,*alternate=nullptr; AVBSFContext *bsf=nullptr,*alternateBSF=nullptr;
    AVPacket *packet=av_packet_alloc(),*alternatePacket=av_packet_alloc();
    try {
        if((argc<4 || argc>6) || avformat_open_input(&input,argv[1],nullptr,nullptr)<0) throw std::runtime_error("fixture input");
        std::string fault=argc>=5?argv[4]:"";
        std::signal(SIGTERM,stop); std::signal(SIGINT,stop);
        auto options=capture_default_options(); options.codec=static_cast<capture_codec>(std::stoi(argv[2])); options.port=std::stoul(argv[3]);
        capture::LocalTCPServer server(uint16_t(options.port)); capture::MediaPipeline pipeline(options,server,[]{});
        auto sink=pipeline.sink(); if(!sink.video_packet || !sink.audio_packet) throw std::runtime_error("pipeline sink unavailable");
        auto filter=av_bsf_get_by_name(options.codec==CAPTURE_H264?"h264_mp4toannexb":"hevc_mp4toannexb");
        if(av_bsf_alloc(filter,&bsf)<0 || avcodec_parameters_copy(bsf->par_in,input->streams[0]->codecpar)<0 || av_bsf_init(bsf)<0) throw std::runtime_error("fixture conversion");
        if(argc==6 && (avformat_open_input(&alternate,argv[5],nullptr,nullptr)<0 || av_bsf_alloc(filter,&alternateBSF)<0 ||
            avcodec_parameters_copy(alternateBSF->par_in,alternate->streams[0]->codecpar)<0 || av_bsf_init(alternateBSF)<0))
            throw std::runtime_error("alternate fixture conversion");
        ChiakiEncodedAudioConfig audio{1,2,48000,960};
        bool audioConfigured=fault!="late-audio";
        if(audioConfigured) sink.audio_config(&audio,sink.user);
        ChiakiEncodedVideoConfig video{1,static_cast<ChiakiCodec>(options.codec),uint32_t(input->streams[0]->codecpar->width),uint32_t(input->streams[0]->codecpar->height),60,1,bsf->par_out->extradata,size_t(bsf->par_out->extradata_size)};
        sink.video_config(&video,sink.user);
        std::cerr << "READY 127.0.0.1:" << options.port << '\n';
        uint64_t start=chiaki_time_now_monotonic_us(); int64_t epoch=AV_NOPTS_VALUE; uint16_t indices[2]{};
        uint64_t lastOutputResets=0,videoGeneration=1,audioGeneration=1;
        bool paused=false,transitioned=false,reported=false,replayed=false;
        while(!stopped && av_read_frame(input,packet)>=0) {
            int index=packet->stream_index; auto tb=input->streams[index]->time_base;
            int64_t pts=av_rescale_q(packet->pts,tb,AV_TIME_BASE_Q); if(epoch==AV_NOPTS_VALUE) epoch=pts;
            if(index==0 && alternate) {
                bool found=false;
                while(av_read_frame(alternate,alternatePacket)>=0) {
                    if(alternatePacket->stream_index==0) {found=true;break;}
                    av_packet_unref(alternatePacket);
                }
                if(!found)throw std::runtime_error("alternate video exhausted");
            }
            while(!stopped && chiaki_time_now_monotonic_us()<start+uint64_t(pts-epoch)) poll(nullptr,0,1);
            if(stopped) break;
            if(fault=="source-pause" && !paused && pts-epoch>=2000000) {
                auto resume=chiaki_time_now_monotonic_us()+350000;
                while(!stopped && chiaki_time_now_monotonic_us()<resume) poll(nullptr,0,1);
                paused=true;if(stopped)break;
            }
            if(!audioConfigured && pts-epoch>=500000) {sink.audio_config(&audio,sink.user);audioConfigured=true;}
            if(fault=="transition-generation" && !transitioned && pts-epoch>=2000000) {
                video.generation=++videoGeneration;audio.generation=++audioGeneration;
                sink.video_config(&video,sink.user);sink.audio_config(&audio,sink.user);transitioned=true;
            }
            if(index==0 && (av_bsf_send_packet(bsf,packet)<0 || av_bsf_receive_packet(bsf,packet)<0)) throw std::runtime_error("fixture Annex-B conversion");
            std::vector<uint8_t> oldHeaders;
            if(index==0 && alternate && pts-epoch>=2000000) {
                if(av_bsf_send_packet(alternateBSF,alternatePacket)<0 || av_bsf_receive_packet(alternateBSF,alternatePacket)<0)
                    throw std::runtime_error("alternate Annex-B conversion");
                if(!transitioned) {
                    if(fault=="transition-inband-reorder")oldHeaders.assign(packet->data,packet->data+packet->size);
                    if(fault=="transition-profile") {
                        video.generation=++videoGeneration;
                        video.header=alternateBSF->par_out->extradata;video.header_size=alternateBSF->par_out->extradata_size;
                        sink.video_config(&video,sink.user);
                    }
                    transitioned=true;
                }
                av_packet_unref(packet);av_packet_move_ref(packet,alternatePacket);
            }
            std::vector<uint8_t> altered;
            if(index==0 && fault=="transition-hdr" && pts-epoch>=2000000) {
                altered=withoutHEVCSEI(packet->data,packet->size);
                auto access=capture::inspectAccessUnit(CHIAKI_CODEC_H265_HDR,altered.data(),altered.size()).access;
                if(!transitioned && (access==capture::VideoAccess::IDR || access==capture::VideoAccess::CRA)) {
                    const uint8_t metadata[]{0,0,0,1,0x4e,1,144,4,7,0xd0,1,0x90,0x80};
                    altered.insert(altered.begin(),std::begin(metadata),std::end(metadata));transitioned=true;
                }
            }
            ChiakiEncodedPacket p{index==0?videoGeneration:audioGeneration,chiaki_time_now_monotonic_us(),indices[index]++,0,index==1?960u:0u,0,
                altered.empty()?packet->data:altered.data(),altered.empty()?size_t(packet->size):altered.size()};
            if(index==0) sink.video_packet(&p,sink.user);
            else if(audioConfigured && fault!="missing-audio") sink.audio_packet(&p,sink.user);
            if(!replayed && !oldHeaders.empty()) {
                auto old=p;old.frame_index=uint16_t(p.frame_index-1);old.data=oldHeaders.data();old.size=oldHeaders.size();
                sink.video_packet(&old,sink.user);replayed=true;
            }
            av_packet_unref(packet);
            av_packet_unref(alternatePacket);
            auto status=pipeline.snapshot();
            if(status.status.output_disconnects!=lastOutputResets) {
                lastOutputResets=status.status.output_disconnects;
                std::cerr<<"OUTPUT_RESET "<<lastOutputResets<<' '<<status.status.message<<'\n';
            }
            if(status.done) {std::cerr<<"SOURCE_FAILURE retry="<<status.retry<<'\n';throw std::runtime_error(status.status.message);}
            if(fault.rfind("transition-",0)==0 && !reported && pts-epoch>=4000000) {
                std::cerr<<"TRANSITION_STATUS "<<status.status.output_disconnects<<' '<<status.status.audio_clock_position<<' '
                         <<status.status.audio_packets<<' '<<status.status.video_packets<<'\n';reported=true;
            }
        }
        while(!stopped) poll(nullptr,0,10);
        pipeline.stopAndJoin(); av_packet_free(&packet);av_packet_free(&alternatePacket);av_bsf_free(&bsf);av_bsf_free(&alternateBSF);
        avformat_close_input(&input);avformat_close_input(&alternate);return 0;
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; }
    av_packet_free(&packet);av_packet_free(&alternatePacket);av_bsf_free(&bsf);av_bsf_free(&alternateBSF);
    avformat_close_input(&input);avformat_close_input(&alternate);return 1;
}
