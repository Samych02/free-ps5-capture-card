#ifndef CAPTURE_ENGINE_H
#define CAPTURE_ENGINE_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32) && !defined(CAPTURE_ENGINE_STATIC)
#if defined(CAPTURE_ENGINE_BUILD)
#define CAPTURE_API __declspec(dllexport)
#else
#define CAPTURE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define CAPTURE_API __attribute__((visibility("default")))
#else
#define CAPTURE_API
#endif
#define CAPTURE_API_VERSION 3
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
typedef enum { CAPTURE_IDLE, CAPTURE_STOPPED, CAPTURE_CONNECTING, CAPTURE_WAITING_OBS,
    CAPTURE_STREAMING, CAPTURE_RETRYING, CAPTURE_PIN_REQUIRED, CAPTURE_FAILED } capture_state;
typedef enum { CAPTURE_ERROR_NONE, CAPTURE_ERROR_PORT, CAPTURE_ERROR_CONNECTION,
    CAPTURE_ERROR_IDENTITY, CAPTURE_ERROR_IN_USE, CAPTURE_ERROR_MEDIA,
    CAPTURE_ERROR_OVERFLOW, CAPTURE_ERROR_INTERNAL } capture_error;
/* Presentation facts, not inferred from translated or historical messages. */
typedef enum { CAPTURE_PHASE_NONE, CAPTURE_PHASE_WAITING_MEDIA,
    CAPTURE_PHASE_WAITING_OBS, CAPTURE_PHASE_STARTING_OUTPUT, CAPTURE_PHASE_STREAMING,
    CAPTURE_PHASE_MEDIA_PAUSED, CAPTURE_PHASE_CONTENT_BLOCKED } capture_phase;
typedef struct {
    uint32_t size, version;
    capture_state state;
    capture_error error;
    uint32_t width, height, fps_num, fps_den, port;
    capture_codec codec;
    uint64_t video_packets, audio_packets, reconnects, queued_bytes;
    uint64_t lost_video_frames, missing_audio_packets, queue_age_us;
    uint64_t output_disconnects;
    int64_t video_clock_drift_us, audio_clock_drift_us;
    uint64_t video_clock_elapsed_us, audio_clock_elapsed_us;
    uint64_t video_clock_position, audio_clock_position;
    uint32_t audio_samples_per_packet;
    capture_phase phase;
    uint64_t pin_request_id; /* new request/rejection identity; zero outside a PIN prompt */
    char message[192]; /* current issue only; sanitized, no protocol strings or credentials */
    char last_output_message[192], last_source_message[192]; /* historical diagnostics */
} capture_status;
/* Callback data is borrowed; copy it before dispatching elsewhere. Callbacks
 * run on the engine worker. Never block them on the UI/main thread. */
typedef void (*capture_callback)(capture_state state, void *context);
CAPTURE_API capture_options capture_default_options(void);
CAPTURE_API capture_result capture_validate_options(const capture_options *options);
CAPTURE_API capture_engine *capture_engine_create(const capture_options *options,
    capture_callback callback, void *context, capture_result *error);
/* Nonblocking and repeatable. Does not sleep/shut down the console. */
CAPTURE_API capture_result capture_engine_stop(capture_engine *engine);
/* Requires a configured connection; starts asynchronously. */
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
typedef struct {
    uint32_t size, version;
    char address[64];
    uint8_t account_id[8];
    capture_registration registration;
} capture_connection;
/* Copied, pre-start only. Existing credentials survive invalid replacement.
 * Stop wipes the engine's copy; configure again before the next Start. */
CAPTURE_API capture_result capture_engine_configure(capture_engine *, const capture_options *, const capture_connection *);
CAPTURE_API capture_result capture_engine_get_status(capture_engine *, capture_status *);
/* Exactly four ASCII digits, transient, only while a login passcode is requested. */
CAPTURE_API capture_result capture_engine_submit_login_pin(capture_engine *, const char *, size_t);
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
