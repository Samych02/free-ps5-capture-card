#include "Pipeline.h"
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
int main(int argc,char **argv) {
    AVFormatContext *input=nullptr; AVBSFContext *bsf=nullptr; AVPacket *packet=av_packet_alloc();
    try {
        if((argc!=4 && argc!=5) || avformat_open_input(&input,argv[1],nullptr,nullptr)<0) throw std::runtime_error("fixture input");
        std::string fault=argc==5?argv[4]:"";
        std::signal(SIGTERM,stop); std::signal(SIGINT,stop);
        auto options=capture_default_options(); options.codec=static_cast<capture_codec>(std::stoi(argv[2])); options.port=std::stoul(argv[3]);
        capture::LocalTCPServer server(uint16_t(options.port)); capture::MediaPipeline pipeline(options,server,[]{});
        auto sink=pipeline.sink(); if(!sink.video_packet || !sink.audio_packet) throw std::runtime_error("pipeline sink unavailable");
        auto filter=av_bsf_get_by_name(options.codec==CAPTURE_H264?"h264_mp4toannexb":"hevc_mp4toannexb");
        if(av_bsf_alloc(filter,&bsf)<0 || avcodec_parameters_copy(bsf->par_in,input->streams[0]->codecpar)<0 || av_bsf_init(bsf)<0) throw std::runtime_error("fixture conversion");
        ChiakiEncodedAudioConfig audio{1,2,48000,960};
        bool audioConfigured=fault!="late-audio";
        if(audioConfigured) sink.audio_config(&audio,sink.user);
        ChiakiEncodedVideoConfig video{1,static_cast<ChiakiCodec>(options.codec),uint32_t(input->streams[0]->codecpar->width),uint32_t(input->streams[0]->codecpar->height),60,1,bsf->par_out->extradata,size_t(bsf->par_out->extradata_size)};
        sink.video_config(&video,sink.user);
        std::cerr << "READY 127.0.0.1:" << options.port << '\n';
        uint64_t start=chiaki_time_now_monotonic_us(); int64_t epoch=AV_NOPTS_VALUE; uint16_t indices[2]{};
        uint64_t lastOutputResets=0;bool paused=false;
        while(!stopped && av_read_frame(input,packet)>=0) {
            int index=packet->stream_index; auto tb=input->streams[index]->time_base;
            int64_t pts=av_rescale_q(packet->pts,tb,AV_TIME_BASE_Q); if(epoch==AV_NOPTS_VALUE) epoch=pts;
            while(!stopped && chiaki_time_now_monotonic_us()<start+uint64_t(pts-epoch)) poll(nullptr,0,1);
            if(stopped) break;
            if(fault=="source-pause" && !paused && pts-epoch>=2000000) {
                auto resume=chiaki_time_now_monotonic_us()+350000;
                while(!stopped && chiaki_time_now_monotonic_us()<resume) poll(nullptr,0,1);
                paused=true;if(stopped)break;
            }
            if(!audioConfigured && pts-epoch>=500000) {sink.audio_config(&audio,sink.user);audioConfigured=true;}
            if(index==0 && (av_bsf_send_packet(bsf,packet)<0 || av_bsf_receive_packet(bsf,packet)<0)) throw std::runtime_error("fixture Annex-B conversion");
            ChiakiEncodedPacket p{1,chiaki_time_now_monotonic_us(),indices[index]++,0,index==1?960u:0u,0,packet->data,size_t(packet->size)};
            if(index==0) sink.video_packet(&p,sink.user);
            else if(audioConfigured && fault!="missing-audio") sink.audio_packet(&p,sink.user);
            av_packet_unref(packet);
            auto status=pipeline.snapshot();
            if(status.status.output_disconnects!=lastOutputResets) {
                lastOutputResets=status.status.output_disconnects;
                std::cerr<<"OUTPUT_RESET "<<lastOutputResets<<' '<<status.status.message<<'\n';
            }
            if(status.done) throw std::runtime_error(status.status.message);
        }
        while(!stopped) poll(nullptr,0,10);
        pipeline.stopAndJoin(); av_packet_free(&packet); av_bsf_free(&bsf); avformat_close_input(&input); return 0;
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; }
    av_packet_free(&packet); av_bsf_free(&bsf); avformat_close_input(&input); return 1;
}
