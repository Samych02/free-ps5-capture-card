#include "MatroskaMuxer.h"
#include "LocalTCPServer.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <csignal>
#include <chrono>
#include <poll.h>

static volatile sig_atomic_t stopped = 0;
static void stop(int) { stopped = 1; }
static bool cancelled() { return stopped != 0; }

static void replay(const std::string &fixture, const capture::MatroskaMuxer::Writer &writer,
                   bool loop, bool realtime) {
    AVFormatContext *input = nullptr;
    AVPacket *packet = nullptr;
    try {
        // Matroska headers supply codec parameters. Do not call decoder-based
        // stream probing or open any codec context in this sender.
        if(avformat_open_input(&input, fixture.c_str(), nullptr, nullptr) < 0)
            throw std::runtime_error("Cannot read fixture");
        std::vector<capture::Track> tracks;
        for(unsigned i = 0; i < input->nb_streams; ++i)
            tracks.push_back({input->streams[i]->codecpar, input->streams[i]->time_base, input->streams[i]->avg_frame_rate});
        capture::MatroskaMuxer mux(tracks, writer);
        packet = av_packet_alloc();
        if(!packet) throw std::bad_alloc();
        auto start = std::chrono::steady_clock::now();
        int64_t epoch = 0;
        do {
            int status;
            while(!cancelled() && (status = av_read_frame(input, packet)) >= 0) {
                auto timeBase = input->streams[packet->stream_index]->time_base;
                auto offset = av_rescale_q(epoch, AV_TIME_BASE_Q, timeBase);
                if(packet->pts != AV_NOPTS_VALUE) packet->pts += offset;
                if(packet->dts != AV_NOPTS_VALUE) packet->dts += offset;
                auto timestamp = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;
                if(timestamp == AV_NOPTS_VALUE) throw std::runtime_error("Fixture needs timestamps");
                auto due = start + std::chrono::microseconds(av_rescale_q(timestamp, timeBase, AV_TIME_BASE_Q));
                while(realtime && !cancelled() && std::chrono::steady_clock::now() < due)
                    poll(nullptr, 0, 5);
                if(cancelled()) break;
                mux.write(packet);
                av_packet_unref(packet);
            }
            if(cancelled()) break;
            if(status != AVERROR_EOF) throw std::runtime_error("Error reading fixture packets");
            if(!loop) { mux.finish(); break; }
            if(input->duration <= 0 || input->duration == AV_NOPTS_VALUE ||
               av_seek_frame(input, -1, 0, AVSEEK_FLAG_BACKWARD) < 0)
                throw std::runtime_error("Fixture cannot loop");
            epoch += input->duration;
        } while(!cancelled());
    } catch(...) {
        av_packet_free(&packet); avformat_close_input(&input); throw;
    }
    av_packet_free(&packet);
    avformat_close_input(&input);
}

int main(int argc, char **argv) {
    FILE *file = nullptr;
    int result = 0;
    try {
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        std::string fixture, output;
        unsigned port = 9295;
        bool loop = false, explicitPort = false;
        for(int i = 1; i < argc; ++i) {
            std::string option = argv[i];
            if((option == "--fixture" || option == "--output") && i + 1 < argc) {
                (option == "--fixture" ? fixture : output) = argv[++i];
            } else if(option == "--loop") loop = true;
            else if(option == "--port" && i + 1 < argc) {
                std::string value = argv[++i];
                if(value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                    throw std::runtime_error("Invalid port");
                unsigned long parsed = std::stoul(value);
                if(parsed < 1024 || parsed > 65535) throw std::runtime_error("Port must be 1024..65535");
                port = static_cast<unsigned>(parsed); explicitPort = true;
            } else throw std::runtime_error("Usage: capture-replay --fixture FILE [--port PORT --loop | --output FILE]");
        }
        if(fixture.empty() || fixture == output || (!output.empty() && (loop || explicitPort)))
            throw std::runtime_error("Specify a fixture and either TCP options or a distinct output file");
        if(!output.empty()) {
            file = fopen(output.c_str(), "wbx");
            if(!file) throw std::runtime_error("Cannot create output (existing files are never overwritten)");
            replay(fixture, [&](const uint8_t *data, int size) {
                return fwrite(data, 1, size, file) == static_cast<size_t>(size) ? size : AVERROR(EIO);
            }, false, false);
        } else {
            capture::LocalTCPServer server(static_cast<uint16_t>(port));
            std::cerr << "READY 127.0.0.1:" << port << std::endl;
            while(server.acceptClient(cancelled)) {
                bool networkError = false;
                try {
                    replay(fixture, [&](const uint8_t *data, int size) {
                        int sent = server.write(data, size, cancelled);
                        if(sent < 0) networkError = true;
                        return sent;
                    }, loop, true);
                } catch(const std::exception &error) {
                    if(!networkError && !cancelled()) throw;
                }
                server.disconnect();
            }
        }
    } catch(const std::exception &error) { std::cerr << error.what() << '\n'; result = 1; }
    if(file && fclose(file)) result = 1;
    return result;
}
