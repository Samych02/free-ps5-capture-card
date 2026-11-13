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
#ifdef __cplusplus
}
#endif
#endif
