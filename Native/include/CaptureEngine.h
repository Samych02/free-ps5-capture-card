#ifndef CAPTURE_ENGINE_H
#define CAPTURE_ENGINE_H
#include <stdint.h>
#include <stddef.h>
#define CAPTURE_API __attribute__((visibility("default")))
#define CAPTURE_API_VERSION 1
#ifdef __cplusplus
extern "C" {
#endif
typedef struct capture_engine capture_engine;
typedef enum { CAPTURE_OK, CAPTURE_INVALID_OPTIONS, CAPTURE_INVALID_STATE,
               CAPTURE_UNAVAILABLE, CAPTURE_INTERNAL_ERROR } capture_result;
typedef enum { CAPTURE_H264, CAPTURE_H265, CAPTURE_H265_HDR } capture_codec;
typedef struct {
    uint32_t size;
    uint32_t version;
    uint32_t width, height, fps, bitrate_kbps, port;
    capture_codec codec;
} capture_options;
typedef enum { CAPTURE_IDLE, CAPTURE_STOPPED } capture_state;
/* Callback data is borrowed; copy it before dispatching elsewhere. Callbacks
 * run on the engine worker. Never block them on the UI/main thread. */
typedef void (*capture_callback)(capture_state state, void *context);
CAPTURE_API capture_options capture_default_options(void);
CAPTURE_API capture_result capture_validate_options(const capture_options *options);
CAPTURE_API capture_engine *capture_engine_create(const capture_options *options,
    capture_callback callback, void *context, capture_result *error);
/* Nonblocking and repeatable. Does not sleep/shut down the console. */
CAPTURE_API capture_result capture_engine_stop(capture_engine *engine);
/* Capture is not implemented in this milestone; returns CAPTURE_UNAVAILABLE. */
CAPTURE_API capture_result capture_engine_start(capture_engine *engine);
/* Call off the main thread, outside a callback; joins before freeing. Context
 * must remain alive until this returns. Returns INVALID_STATE from a callback. */
CAPTURE_API capture_result capture_engine_destroy(capture_engine *engine);

/* Local PS5 setup only. Fixed-size records are owned values, with no borrowed
 * Chiaki pointers or AP/Wi-Fi secrets. Target is the pinned core target ID. */
typedef struct {
    char address[64], name[64], id[13];
    uint32_t target;
    uint32_t state; /* 0 unknown, 1 ready, 2 standby */
} capture_console;
typedef struct {
    uint32_t size, version;
    char address[64];
    uint8_t account_id[8];
    char pin[9]; /* exactly eight ASCII digits, including leading zeroes */
    uint32_t target;
} capture_pairing_options;
typedef struct {
    uint32_t target;
    uint8_t mac[6];
    char name[64];
    uint8_t registration_key[16], session_key[16];
    uint32_t key_type;
} capture_registration;
typedef enum { CAPTURE_SETUP_OK, CAPTURE_SETUP_CANCELLED,
    CAPTURE_SETUP_FAILED, CAPTURE_SETUP_PERMISSION_DENIED } capture_setup_status;
typedef struct capture_pairing capture_pairing;
typedef struct capture_discovery capture_discovery;
/* Callbacks run on background workers. Copy data before returning. Never call
 * destroy from a callback. Stop is nonblocking; destroy stops and joins and
 * must run off the UI thread. Context remains alive through destroy. */
typedef void (*capture_pairing_callback)(capture_setup_status,
    const capture_registration *, void *);
typedef void (*capture_discovery_callback)(capture_setup_status,
    const capture_console *, size_t count, void *);
CAPTURE_API capture_result capture_validate_pairing(const capture_pairing_options *);
CAPTURE_API capture_pairing *capture_pairing_create(const capture_pairing_options *,
    capture_pairing_callback, void *, capture_result *);
CAPTURE_API void capture_pairing_stop(capture_pairing *);
CAPTURE_API capture_result capture_pairing_destroy(capture_pairing *);
CAPTURE_API capture_discovery *capture_discovery_create(capture_discovery_callback,
    void *, capture_result *);
CAPTURE_API void capture_discovery_stop(capture_discovery *);
CAPTURE_API capture_result capture_discovery_destroy(capture_discovery *);
#ifdef __cplusplus
}
#endif
#endif
