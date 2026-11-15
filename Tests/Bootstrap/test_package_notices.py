from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PackageNoticeTests(unittest.TestCase):
    def test_packager_copies_curl_copyright_and_permission_notice(self):
        # Exercise the actual embedded notice-copy program without a native build.
        script = (ROOT / 'scripts/package-dev-app.sh').read_text().split("<<'PY'\n", 1)[1].split('\nPY\n', 1)[0]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'build/native/Release/_deps/chiaki-src'
            for name in ('', 'third-party/gf-complete', 'third-party/jerasure', 'third-party/nanopb', 'third-party/curl'):
                directory = source / name
                directory.mkdir(parents=True, exist_ok=True)
                (directory / 'COPYING').write_text(f'Complete {name or "chiaki"} license notice\n')
            (root / 'Dependencies.lock.json').write_text('{}')
            app = root / 'Test.app'
            subprocess.run([sys.executable, '-', str(root), str(app), 'Release'], input=script, text=True, check=True)
            copied = app / 'Contents/Resources/ProjectNotices/EmbeddedSources/curl/COPYING'
            self.assertTrue(copied.is_file(), 'Statically linked curl must carry its license notice')
            self.assertEqual(copied.read_bytes(), (source / 'third-party/curl/COPYING').read_bytes())

    def test_bundle_validator_rejects_missing_curl_notices(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / 'Test.app'
            (app / 'Contents/Frameworks/CaptureEngine.framework').mkdir(parents=True)
            for name in ('chiaki', 'gf-complete', 'jerasure', 'nanopb'):
                (app / 'Contents/Resources/ProjectNotices/EmbeddedSources' / name).mkdir(parents=True)
            result = subprocess.run([sys.executable, str(ROOT / 'Tests/Support/check_bundle.py'), '--app', str(app)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Missing embedded-source license notices: curl', result.stderr)
