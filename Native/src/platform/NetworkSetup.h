#pragma once
#include <chiaki/discoveryservice.h>
#include <vector>

namespace capture::platform {
// OS implementations validate literal console addresses and own discovery
// destinations. apply() lends stable storage through discovery-service fini.
bool validConsoleAddress(const char *);
class DiscoveryAddresses {
    sockaddr_storage destination_{};
    std::vector<sockaddr_storage> broadcasts_;
public:
    DiscoveryAddresses();
    DiscoveryAddresses(const DiscoveryAddresses &) = delete;
    DiscoveryAddresses &operator=(const DiscoveryAddresses &) = delete;
    void apply(ChiakiDiscoveryServiceOptions &);
};
}
