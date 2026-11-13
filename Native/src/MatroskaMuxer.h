#pragma once
#include <functional>
#include <vector>
extern "C" {
#include <libavformat/avformat.h>
}

namespace capture {
struct Track { const AVCodecParameters *parameters; AVRational timeBase; AVRational frameRate; };
class MatroskaMuxer {
public:
    using Writer = std::function<int(const uint8_t *, int)>;
    MatroskaMuxer(const std::vector<Track> &tracks, Writer writer, bool orderedLive = false);
    ~MatroskaMuxer();
    MatroskaMuxer(const MatroskaMuxer &) = delete;
    MatroskaMuxer &operator=(const MatroskaMuxer &) = delete;
    void write(const AVPacket *packet);
    void finish();
private:
    AVFormatContext *format_ = nullptr;
    AVIOContext *io_ = nullptr;
    Writer writer_;
    std::vector<AVRational> timeBases_;
    bool finished_ = false;
    bool orderedLive_ = false;
    std::vector<int64_t> lastDts_;
    int64_t firstDts_ = AV_NOPTS_VALUE, globalDts_ = AV_NOPTS_VALUE;
    static int output(void *, const uint8_t *, int);
    void release();
};
}
