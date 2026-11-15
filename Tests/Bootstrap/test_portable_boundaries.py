from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PortableBoundaryTests(unittest.TestCase):
    def test_shared_native_code_has_no_os_headers_or_compiler_wide_integer(self):
        forbidden = r'#include\s*[<"](?:arpa/|sys/socket|netinet/|ifaddrs|net/if|poll\.h)|\b__int128\b'
        for path in sorted((ROOT / 'Native/src').glob('*')):
            if path.suffix in ('.cpp', '.h'):
                self.assertIsNone(re.search(forbidden, path.read_text()), str(path.relative_to(ROOT)))

    def test_framework_packaging_is_apple_only(self):
        cmake = (ROOT / 'Native/CMakeLists.txt').read_text()
        self.assertRegex(cmake, r'if\(APPLE\)\s+set_target_properties\(CaptureEngine')

    def test_public_api_exports_have_a_windows_boundary(self):
        header = (ROOT / 'Native/include/CaptureEngine.h').read_text()
        self.assertIn('defined(_WIN32)', header)
        self.assertIn('CAPTURE_ENGINE_BUILD', header)
        self.assertIn('__declspec(dllexport)', header)
