#pragma once
#include <functional>
#include <cstdint>

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
    int listener_ = -1, client_ = -1;
};
}
