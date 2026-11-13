#include "LiveFormat.h"
#include "MatroskaMuxer.h"
#include <iostream>
#include <stdexcept>
extern "C" {
#include <libavcodec/bsf.h>
#include <libavutil/mastering_display_metadata.h>
}
int main(int argc, char **argv) {
    AVFormatContext *input = nullptr; AVBSFContext *bsf = nullptr; AVPacket *packet = av_packet_alloc();
    try {
        if((argc != 3 && argc != 4) || avformat_open_input(&input,argv[1],nullptr,nullptr)<0) throw std::runtime_error("fixture input");
        auto codec = static_cast<ChiakiCodec>(std::stoi(argv[2]));
        auto filter = av_bsf_get_by_name(codec == CHIAKI_CODEC_H264 ? "h264_mp4toannexb" : "hevc_mp4toannexb");
        if(av_bsf_alloc(filter,&bsf)<0 || avcodec_parameters_copy(bsf->par_in,input->streams[0]->codecpar)<0 || av_bsf_init(bsf)<0)
            throw std::runtime_error("fixture Annex-B filter");
        int64_t firstAudio=AV_NOPTS_VALUE;
        while(av_read_frame(input,packet)>=0) {
            if(packet->stream_index != 0) {
                if(packet->stream_index==1 && firstAudio==AV_NOPTS_VALUE) firstAudio=av_rescale_q(packet->dts,input->streams[1]->time_base,AV_TIME_BASE_Q);
                av_packet_unref(packet); continue;
            }
            int64_t firstVideo=av_rescale_q(packet->pts,input->streams[0]->time_base,AV_TIME_BASE_Q);
            if(av_bsf_send_packet(bsf,packet)<0 || av_bsf_receive_packet(bsf,packet)<0) throw std::runtime_error("fixture conversion");
            auto format = capture::parseVideoFormat(codec,bsf->par_out->extradata,bsf->par_out->extradata_size,packet->data,packet->size);
            if(!format.parameters) throw std::runtime_error("live metadata is missing");
            if(format.hdr) {
                if(!capture::hdrMetadataMatches(format.parameters.get(),packet->data,packet->size)) throw std::runtime_error("matching HDR metadata rejected");
                capture::Parameters changed(avcodec_parameters_alloc()); avcodec_parameters_copy(changed.get(),format.parameters.get());
                auto sd=av_packet_side_data_get(changed->coded_side_data,changed->nb_coded_side_data,AV_PKT_DATA_CONTENT_LIGHT_LEVEL);
                if(!sd) throw std::runtime_error("missing fixture content light");
                reinterpret_cast<AVContentLightMetadata *>(sd->data)->MaxCLL++;
                if(capture::hdrMetadataMatches(changed.get(),packet->data,packet->size)) throw std::runtime_error("HDR metadata change not detected");
            }
            auto audio = capture::liveAudioParameters({1,2,48000,960});
            if(!audio || audio->initial_padding != 0 || audio->extradata_size != 19) throw std::runtime_error("live audio parameters");
            std::cout << "{\"width\":" << format.parameters->width << ",\"height\":" << format.parameters->height
                << ",\"fps_num\":" << format.frameRate.num << ",\"fps_den\":" << format.frameRate.den
                << ",\"depth\":" << format.depth << ",\"hdr\":" << (format.hdr?"true":"false")
                << ",\"primaries\":" << format.parameters->color_primaries << ",\"transfer\":" << format.parameters->color_trc
                << ",\"side_data\":" << format.parameters->nb_coded_side_data << "}\n";
            if(argc==4) {
                if(std::string(argv[3])=="--check-bounds") {
                    capture::MatroskaMuxer mux({{format.parameters.get(),AV_TIME_BASE_Q,format.frameRate},{audio.get(),AV_TIME_BASE_Q,{0,1}}},
                        [](const uint8_t *,int n) { return n; },true);
                    AVPacket giant{}; giant.data=reinterpret_cast<uint8_t *>(1); giant.size=4*1024*1024+1; giant.pts=giant.dts=0;
                    bool rejected=false; try { mux.write(&giant); } catch(const std::runtime_error &) { rejected=true; }
                    if(!rejected) throw std::runtime_error("Oversized mux packet was accepted");
                    av_packet_free(&packet); av_bsf_free(&bsf); avformat_close_input(&input); return 0;
                }
                av_packet_unref(packet);
                while(firstAudio==AV_NOPTS_VALUE && av_read_frame(input,packet)>=0) {
                    if(packet->stream_index==1) firstAudio=av_rescale_q(packet->dts,input->streams[1]->time_base,AV_TIME_BASE_Q);
                    av_packet_unref(packet);
                }
                if(firstAudio==AV_NOPTS_VALUE) throw std::runtime_error("missing fixture audio");
                int64_t epoch=std::min(firstAudio,firstVideo);
                av_bsf_free(&bsf); avformat_close_input(&input);
                if(avformat_open_input(&input,argv[1],nullptr,nullptr)<0 || av_bsf_alloc(filter,&bsf)<0 ||
                   avcodec_parameters_copy(bsf->par_in,input->streams[0]->codecpar)<0 || av_bsf_init(bsf)<0) throw std::runtime_error("fixture reopen");
                auto file=std::unique_ptr<FILE,decltype(&fclose)>(fopen(argv[3],"wbx"),fclose);
                if(!file) throw std::runtime_error("fixture output already exists or cannot open");
                uint64_t inputBytes=0,outputBytes=0,largest=0;
                capture::MatroskaMuxer mux({{format.parameters.get(),AV_TIME_BASE_Q,format.frameRate},{audio.get(),AV_TIME_BASE_Q,{0,1}}},
                    [&](const uint8_t *p,int n) { outputBytes+=n; return fwrite(p,1,n,file.get())==size_t(n)?n:AVERROR(EIO); },true);
                while(av_read_frame(input,packet)>=0) {
                    int index=packet->stream_index; auto tb=input->streams[index]->time_base;
                    if(index==0 && (av_bsf_send_packet(bsf,packet)<0 || av_bsf_receive_packet(bsf,packet)<0)) throw std::runtime_error("fixture video conversion");
                    // The live parser has independently established no reorder;
                    // Matroska demux may omit initial H264 DTS without probing.
                    if(packet->dts==AV_NOPTS_VALUE) packet->dts=packet->pts;
                    if(packet->pts==AV_NOPTS_VALUE) throw std::runtime_error("Missing fixture presentation timestamp");
                    av_packet_free_side_data(packet); // emulate wire packets, not fixture encoder skip/discard side data
                    av_packet_rescale_ts(packet,tb,AV_TIME_BASE_Q); packet->pts-=epoch; packet->dts-=epoch;
                    inputBytes+=packet->size; largest=std::max(largest,uint64_t(packet->size));
                    mux.write(packet);
                    if(inputBytes>outputBytes && inputBytes-outputBytes>largest+131072) throw std::runtime_error("Unbounded mux output buffering");
                    av_packet_unref(packet);
                }
                mux.finish();
            }
            av_packet_free(&packet); av_bsf_free(&bsf); avformat_close_input(&input); return 0;
        }
        throw std::runtime_error("no video packet");
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; }
    av_packet_free(&packet); av_bsf_free(&bsf); avformat_close_input(&input); return 1;
}
