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
MatroskaMuxer::MatroskaMuxer(const std::vector<Track> &tracks, Writer writer) : writer_(std::move(writer)) {
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
    auto copy = av_packet_clone(packet);
    if(!copy) throw std::bad_alloc();
    av_packet_rescale_ts(copy, timeBases_[copy->stream_index], format_->streams[copy->stream_index]->time_base);
    int status = av_interleaved_write_frame(format_, copy);
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
