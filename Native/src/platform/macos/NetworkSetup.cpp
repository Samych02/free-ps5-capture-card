#include "platform/NetworkSetup.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <cstring>

namespace capture::platform {
bool validConsoleAddress(const char *text) {
    in_addr addr4{}; in6_addr addr6{};
    if(inet_pton(AF_INET,text,&addr4)==1) {
        auto ip=ntohl(addr4.s_addr);
        return ip!=0 && ip!=0xffffffff && (ip>>24)!=127 && (ip>>28)<14;
    }
    return inet_pton(AF_INET6,text,&addr6)==1 && !IN6_IS_ADDR_UNSPECIFIED(&addr6)
        && !IN6_IS_ADDR_LOOPBACK(&addr6) && !IN6_IS_ADDR_MULTICAST(&addr6);
}
DiscoveryAddresses::DiscoveryAddresses() {
    auto &address=*reinterpret_cast<sockaddr_in *>(&destination_);
    address.sin_family=AF_INET;address.sin_len=sizeof(address);
    address.sin_addr.s_addr=INADDR_BROADCAST;
    ifaddrs *interfaces=nullptr;
    if(getifaddrs(&interfaces)!=0)return;
    // Release interface storage even when vector allocation throws.
    struct Release { ifaddrs *value; ~Release(){freeifaddrs(value);} } release{interfaces};
    for(auto *i=interfaces;i;i=i->ifa_next) {
        if(!i->ifa_addr || !i->ifa_broadaddr || i->ifa_addr->sa_family!=AF_INET ||
           !(i->ifa_flags&IFF_UP) || !(i->ifa_flags&IFF_BROADCAST) || (i->ifa_flags&IFF_LOOPBACK))continue;
        sockaddr_storage storage{};memcpy(&storage,i->ifa_broadaddr,sizeof(sockaddr_in));
        broadcasts_.push_back(storage);
    }
}
void DiscoveryAddresses::apply(ChiakiDiscoveryServiceOptions &options) {
    options.send_addr=&destination_;options.send_addr_size=sizeof(sockaddr_in);
    options.broadcast_addrs=broadcasts_.data();options.broadcast_num=broadcasts_.size();
}
}
