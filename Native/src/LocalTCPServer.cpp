#include "LocalTCPServer.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <chrono>
#include <system_error>
extern "C" {
#include <libavutil/error.h>
}

namespace capture {
static void nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::system_error(errno, std::generic_category(), "nonblocking socket");
}
LocalTCPServer::LocalTCPServer(uint16_t port) {
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listener_ < 0) throw std::system_error(errno, std::generic_category(), "socket");
    try {
        int yes = 1;
        setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if(bind(listener_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 || listen(listener_, 1) < 0)
            throw std::system_error(errno, std::generic_category(), "bind/listen localhost port");
        nonblocking(listener_);
    } catch(...) { close(listener_); listener_ = -1; throw; }
}
bool LocalTCPServer::acceptClient(const std::function<bool()> &cancelled) {
    disconnect();
    while(!cancelled()) {
        pollfd fd{listener_, POLLIN, 0};
        int ready = poll(&fd, 1, 50);
        if(ready < 0 && errno == EINTR) continue;
        if(ready < 0) throw std::system_error(errno, std::generic_category(), "accept poll");
        if(!ready) continue;
        client_ = accept(listener_, nullptr, nullptr);
        if(client_ < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "accept");
        }
        try {
            nonblocking(client_);
            int yes = 1, buffer = 65536;
            if(setsockopt(client_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0 ||
               setsockopt(client_, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) < 0 ||
               setsockopt(client_, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer)) < 0)
                throw std::system_error(errno, std::generic_category(), "client socket options");
        } catch(...) { disconnect(); throw; }
        return true;
    }
    return false;
}
int LocalTCPServer::write(const uint8_t *data, int size, const std::function<bool()> &cancelled) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    int offset = 0;
    while(offset < size) {
        if(cancelled()) return AVERROR_EXIT;
        if(client_ < 0) return AVERROR(EPIPE);
        // Reject queued extra consumers rather than letting a TCP handshake
        // imply a second supported stream. Bound the drain under connection floods.
        for(int count = 0; count < 8; ++count) {
            int extra = accept(listener_, nullptr, nullptr);
            if(extra < 0) break;
            close(extra);
        }
        if(std::chrono::steady_clock::now() >= deadline) return AVERROR(ETIMEDOUT);
        auto sent = send(client_, data + offset, static_cast<size_t>(size - offset), 0);
        if(sent > 0) { offset += static_cast<int>(sent); continue; }
        if(sent == 0) return AVERROR(EPIPE);
        if(errno == EINTR) continue;
        if(errno != EAGAIN && errno != EWOULDBLOCK) return AVERROR(errno);
        pollfd fd{client_, POLLOUT, 0};
        int ready = poll(&fd, 1, 25);
        if(ready < 0 && errno != EINTR) return AVERROR(errno);
        if(ready > 0 && (fd.revents & (POLLERR | POLLHUP | POLLNVAL))) return AVERROR(EPIPE);
    }
    return size;
}
void LocalTCPServer::disconnect() {
    if(client_ >= 0) { close(client_); client_ = -1; }
}
bool LocalTCPServer::clientAlive() {
    if(client_<0) return false;
    uint8_t byte; auto n=recv(client_,&byte,1,MSG_PEEK|MSG_DONTWAIT);
    // EOF here means the peer closed its SEND half, not necessarily its receive
    // half. Read-only TCP media clients may do this and still consume our data.
    // A fully closed receiver is detected by bounded writes (EPIPE/reset/timeout).
    return n>=0 || (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR);
}
void LocalTCPServer::rejectExtraClients() {
    for(int count=0;count<8;++count) { int fd=accept(listener_,nullptr,nullptr); if(fd<0) break; close(fd); }
}
LocalTCPServer::~LocalTCPServer() { disconnect(); if(listener_ >= 0) close(listener_); }
}
