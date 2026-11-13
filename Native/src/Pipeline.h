#pragma once
#include "Session.h"
#include "LiveFormat.h"
#include "EncodedMedia.h"
namespace capture {
class MediaPipeline {
public:
    MediaPipeline(capture_options,LocalTCPServer &,std::function<void()> requestKeyframe);
    ~MediaPipeline();
    ChiakiEncodedMediaSink sink();
    SessionSnapshot snapshot();
    void stopAndJoin();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
