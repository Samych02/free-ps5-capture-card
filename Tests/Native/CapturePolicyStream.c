/* Compile the real patched stream implementation with test-only send/lifecycle
 * substitutions. Wrappers expose its internal helpers, not reimplementations. */
#define chiaki_launchspec_format capture_test_launchspec_format
#include "streamconnection.c"
ChiakiErrorCode capture_test_start_feedback(ChiakiStreamConnection *s) { return stream_connection_start_feedback(s); }
void capture_test_stop_feedback(ChiakiStreamConnection *s) { stream_connection_stop_feedback(s); }
ChiakiErrorCode capture_test_input_setup(ChiakiStreamConnection *s) { return stream_connection_send_input_setup(s); }
ChiakiErrorCode capture_test_ack(ChiakiStreamConnection *s) { return stream_connection_send_streaminfo_ack(s); }
ChiakiErrorCode capture_test_heartbeat(ChiakiStreamConnection *s) { return stream_connection_send_heartbeat(s); }
ChiakiErrorCode capture_test_launch(ChiakiStreamConnection *s) { return stream_connection_send_big(s); }
