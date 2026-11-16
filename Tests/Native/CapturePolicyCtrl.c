/* Exercise the real request formatter and normal control writes. Synthetic
 * strings below are not authentication material; never record live requests. */
#include "ctrl.c"
int capture_test_ctrl_request(ChiakiCtrl *ctrl, char *buf, size_t size)
{
	return ctrl_format_request(ctrl, buf, size, "test-auth", "test-device",
			"test-os", "test-bitrate", "test-streaming");
}
