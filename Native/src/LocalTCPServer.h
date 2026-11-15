#pragma once
#include <functional>
#include <cstdint>
#include <memory>

namespace capture {
class LocalTCPServer {
public:
    explicit LocalTCPServer(uint16_t port);
    ~LocalTCPServer();
    LocalTCPServer(const LocalTCPServer &) = delete;
    LocalTCPServer &operator=(const LocalTCPServer &) = delete;
    bool acceptClient(const std::function<bool()> &cancelled);
    int write(const uint8_t *data, int size, const std::function<bool()> &cancelled);
    void disconnect();
    bool clientAlive();
    void rejectExtraClients();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
