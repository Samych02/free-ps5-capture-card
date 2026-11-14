#pragma once
#include "Session.h"
#include "LiveFormat.h"
#include "EncodedMedia.h"
namespace capture {
using VideoFormatParser=std::function<VideoFormat(ChiakiCodec,const uint8_t *,size_t,const uint8_t *,size_t)>;
class MediaPipeline {
public:
    MediaPipeline(capture_options,LocalTCPServer &,std::function<void()> requestKeyframe,VideoFormatParser=parseVideoFormat);
    ~MediaPipeline();
    ChiakiEncodedMediaSink sink();
    SessionSnapshot snapshot();
    void setSourceBlocked(bool);
    void stopAndJoin();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
