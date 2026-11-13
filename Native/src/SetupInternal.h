#pragma once
#include "CaptureEngine.h"
#include <chiaki/regist.h>
#include <chiaki/discoveryservice.h>
#include <vector>

// Injectable library boundary. Tests never contact a real console or network.
struct PairingAPI {
    decltype(&chiaki_regist_start) start;
    decltype(&chiaki_regist_stop) stop;
    decltype(&chiaki_regist_fini) fini;
};
capture_pairing *pairing_create(const capture_pairing_options *, capture_pairing_callback,
    void *, capture_result *, PairingAPI);
bool copy_registration(const ChiakiRegisteredHost &, capture_registration &);
std::vector<capture_console> copy_consoles(ChiakiDiscoveryHost *, size_t);
struct DiscoveryAPI {
    decltype(&chiaki_discovery_service_init) init;
    decltype(&chiaki_discovery_service_fini) fini;
};
capture_discovery *discovery_create(capture_discovery_callback, void *, capture_result *, DiscoveryAPI);
