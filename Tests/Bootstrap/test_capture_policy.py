from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]

class CapturePolicyTests(unittest.TestCase):
    def test_production_sources_do_not_open_media_codecs_or_devices(self):
        forbidden = r'\b(?:avcodec_open2|avcodec_send_packet|avcodec_receive_frame|avcodec_send_frame|avcodec_receive_packet|opus_decoder_create|opus_multistream_decoder_create|AudioQueueNewOutput|AVAudioEngine|chiaki_session_set_controller_state|chiaki_session_connect_microphone)\s*\('
        for base in ('App', 'Native'):
            for path in (ROOT / base).rglob('*'):
                if path.suffix in ('.cpp', '.h', '.swift'):
                    self.assertIsNone(re.search(forbidden, path.read_text()), str(path.relative_to(ROOT)))

    def test_no_release_authentication_bypass(self):
        source = (ROOT / 'App/FreePS5CaptureCardApp.swift').read_text()
        self.assertIn('#if DEBUG', source)
        # No fake account/console is inserted into the real auth/store path.
        self.assertNotIn('Account(decimalID:', source)
        self.assertNotIn('PairedConsole(', source)
