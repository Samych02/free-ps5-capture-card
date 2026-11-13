#include "MatroskaMuxer.h"
#include <stdexcept>
#include <string>
#include <cerrno>
extern "C" {
#include <libavutil/mem.h>
}

namespace capture {
static void check(int code, const char *operation) {
    if(code < 0) {
        char message[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(code, message, sizeof(message));
        throw std::runtime_error(std::string(operation) + ": " + message);
    }
}
int MatroskaMuxer::output(void *context, const uint8_t *data, int size) {
    try { return static_cast<MatroskaMuxer *>(context)->writer_(data, size); }
    catch(...) { return AVERROR(EIO); } // Never unwind through FFmpeg's C stack.
}
MatroskaMuxer::MatroskaMuxer(const std::vector<Track> &tracks, Writer writer, bool orderedLive) : writer_(std::move(writer)), orderedLive_(orderedLive) {
    try {
        if(tracks.size() != 2 || !writer_) throw std::runtime_error("Both video and audio tracks are required");
        bool video = false, audio = false;
        check(avformat_alloc_output_context2(&format_, nullptr, "matroska", nullptr), "allocate muxer");
        for(const auto &track : tracks) {
            if(!track.parameters || track.timeBase.num <= 0 || track.timeBase.den <= 0)
                throw std::runtime_error("Invalid track configuration");
            auto codec = track.parameters->codec_id;
            if(track.parameters->codec_type == AVMEDIA_TYPE_VIDEO &&
               (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_HEVC) && !video) video = true;
            else if(track.parameters->codec_type == AVMEDIA_TYPE_AUDIO && codec == AV_CODEC_ID_OPUS && !audio) audio = true;
            else throw std::runtime_error("Expected H264/HEVC video and Opus audio");
            auto stream = avformat_new_stream(format_, nullptr);
            if(!stream) throw std::bad_alloc();
            check(avcodec_parameters_copy(stream->codecpar, track.parameters), "copy codec parameters");
            stream->time_base = track.timeBase;
            stream->avg_frame_rate = track.frameRate;
            timeBases_.push_back(track.timeBase);
            lastDts_.push_back(AV_NOPTS_VALUE);
        }
        if(!video || !audio) throw std::runtime_error("Missing required media track");
        auto buffer = static_cast<unsigned char *>(av_malloc(32768));
        if(!buffer) throw std::bad_alloc();
        io_ = avio_alloc_context(buffer, 32768, 1, this, nullptr, output, nullptr);
        if(!io_) { av_free(buffer); throw std::bad_alloc(); }
        io_->seekable = 0;
        format_->pb = io_;
        format_->flags |= AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_FLUSH_PACKETS;
        format_->max_interleave_delta = 50000;
        if(orderedLive_) format_->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;
        AVDictionary *options = nullptr;
        av_dict_set(&options, "live", "1", 0);
        av_dict_set(&options, "cluster_time_limit", "50", 0);
        av_dict_set(&options, "cluster_size_limit", "32768", 0);
        int status = avformat_write_header(format_, &options);
        av_dict_free(&options);
        check(status, "write Matroska header");
        avio_flush(io_);
        check(io_->error, "flush header");
    } catch(...) { release(); throw; }
}
void MatroskaMuxer::write(const AVPacket *packet) {
    if(finished_ || !packet || packet->stream_index < 0 ||
       static_cast<size_t>(packet->stream_index) >= timeBases_.size() || packet->size <= 0)
        throw std::runtime_error("Invalid mux packet/state");
    if(orderedLive_) {
        size_t index=static_cast<size_t>(packet->stream_index);
        bool audio=format_->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO;
        int64_t dts=av_rescale_q(packet->dts,timeBases_[index],AV_TIME_BASE_Q);
        bool invalid=packet->pts==AV_NOPTS_VALUE || packet->dts==AV_NOPTS_VALUE || packet->pts!=packet->dts || dts<0 ||
            packet->size>(audio?65536:4*1024*1024) || (lastDts_[index]!=AV_NOPTS_VALUE && dts<=lastDts_[index]) ||
            (globalDts_!=AV_NOPTS_VALUE && dts<globalDts_);
        if(firstDts_==AV_NOPTS_VALUE) firstDts_=dts;
        auto other=lastDts_[1-index];
        if((other==AV_NOPTS_VALUE && dts-firstDts_>250000) || (other!=AV_NOPTS_VALUE && dts-other>250000)) invalid=true;
        if(invalid) throw std::runtime_error("Invalid, stale, reordered or oversized live mux packet");
        lastDts_[index]=dts; globalDts_=dts;
    }
    auto copy = av_packet_clone(packet);
    if(!copy) throw std::bad_alloc();
    av_packet_rescale_ts(copy, timeBases_[copy->stream_index], format_->streams[copy->stream_index]->time_base);
    // The live queue already supplies DTS order and bounded look-ahead. Avoid
    // libavformat's independent interleaving queue; Matroska retains at most its
    // configured cluster plus one bounded packet and its last small audio packet.
    int status = orderedLive_ ? av_write_frame(format_, copy) : av_interleaved_write_frame(format_, copy);
    av_packet_free(&copy);
    check(status, "write media packet");
    avio_flush(io_);
    check(io_->error, "flush media packet");
}
void MatroskaMuxer::finish() {
    if(finished_) return;
    finished_ = true;
    check(av_write_trailer(format_), "finish stream");
    avio_flush(io_);
    check(io_->error, "flush stream");
}
void MatroskaMuxer::release() {
    if(format_) { format_->pb = nullptr; avformat_free_context(format_); format_ = nullptr; }
    if(io_) { av_freep(&io_->buffer); avio_context_free(&io_); }
}
MatroskaMuxer::~MatroskaMuxer() { release(); }
}
