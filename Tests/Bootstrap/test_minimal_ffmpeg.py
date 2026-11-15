import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
import subprocess

ROOT = Path(__file__).resolve().parents[2]


class MinimalFFmpegTests(unittest.TestCase):
    def builder(self):
        path = ROOT / 'scripts/build-ffmpeg.py'
        self.assertTrue(path.is_file(), 'Pinned minimal FFmpeg builder is missing')
        spec = importlib.util.spec_from_file_location('minimal_ffmpeg', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_runtime_profile_has_no_fixture_input_or_codecs(self):
        flags = self.builder().profile_flags(False)
        for required in ('--disable-everything', '--disable-autodetect', '--disable-network',
                         '--disable-programs', '--disable-shared', '--enable-static',
                         '--enable-parser=h264,hevc', '--enable-muxer=matroska'):
            self.assertIn(required, flags)
        self.assertFalse(any(flag.startswith(('--enable-decoder', '--enable-encoder',
                                              '--enable-demuxer', '--enable-protocol')) for flag in flags))

    def test_fixture_profile_only_adds_bounded_input_features(self):
        builder = self.builder()
        extra = set(builder.profile_flags(True)) - set(builder.profile_flags(False))
        self.assertEqual(extra, {'--enable-demuxer=matroska', '--enable-protocol=file',
                                 '--enable-bsf=h264_mp4toannexb,hevc_mp4toannexb'})

    def test_cache_identity_changes_with_source_target_profile_or_toolchain(self):
        identity = self.builder().build_identity
        original = identity('source-a', ['runtime'], 'darwin-arm64-26', 'clang-sdk-a')
        self.assertEqual(original, identity('source-a', ['runtime'], 'darwin-arm64-26', 'clang-sdk-a'))
        for arguments in [('source-b', ['runtime'], 'darwin-arm64-26', 'clang-sdk-a'),
                          ('source-a', ['fixtures'], 'darwin-arm64-26', 'clang-sdk-a'),
                          ('source-a', ['runtime'], 'darwin-x86_64-26', 'clang-sdk-a'),
                          ('source-a', ['runtime'], 'darwin-arm64-26', 'clang-sdk-b')]:
            self.assertNotEqual(original, identity(*arguments))

    def archive(self, directory, name='ffmpeg-test/README', content=b'source'):
        path = directory / 'source.tar.xz'
        with tarfile.open(path, 'w:xz') as archive:
            entry = tarfile.TarInfo(name); entry.size = len(content)
            archive.addfile(entry, io.BytesIO(content))
        return path

    def test_source_checksum_mismatch_never_extracts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); archive = self.archive(root)
            with self.assertRaises(ValueError):
                self.builder().verified_extract(archive, root / 'out', '0' * 64)
            self.assertFalse((root / 'out').exists())

    def test_verified_source_extracts_exact_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); archive = self.archive(root)
            self.builder().verified_extract(archive, root / 'out', hashlib.sha256(archive.read_bytes()).hexdigest())
            self.assertEqual((root / 'out/ffmpeg-test/README').read_bytes(), b'source')

    def test_archive_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); archive = self.archive(root, '../escape')
            with self.assertRaises(ValueError):
                self.builder().verified_extract(archive, root / 'out', hashlib.sha256(archive.read_bytes()).hexdigest())
            self.assertFalse((root / 'escape').exists())

    def test_cmake_refuses_a_missing_pinned_prefix(self):
        helper = ROOT / 'cmake/FFmpegDependency.cmake'
        self.assertTrue(helper.is_file(), 'Explicit FFmpeg CMake selection is missing')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.27)\n'
                'project(FFmpegSelection NONE)\n'
                f'include("{helper}")\n')
            result = subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'objects')],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('CAPTURE_FFMPEG_ROOT', result.stderr)

    def test_package_preserves_minimal_ffmpeg_license_and_build_provenance(self):
        path = ROOT / 'scripts/package-ffmpeg.py'
        self.assertTrue(path.is_file(), 'Static FFmpeg packaging provenance is missing')
        spec = importlib.util.spec_from_file_location('package_ffmpeg', path)
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); prefix = root / 'prefix'; notices = prefix / 'share/capture-ffmpeg'
            notices.mkdir(parents=True)
            for name in ('LICENSE.md', 'COPYING.LGPLv2.1'):
                (notices / name).write_text('complete upstream license')
            manifest = '{"source":{"sha256":"test-hash"},"fixtures":false,"configure":["--disable-everything"]}'
            (prefix / 'capture-build.json').write_text(manifest)
            module.copy_notices(prefix, root / 'notices', 'test-hash')
            self.assertEqual((root / 'notices/LICENSE.md').read_bytes(), (notices / 'LICENSE.md').read_bytes())
            self.assertEqual((root / 'notices/capture-build.json').read_text(), manifest)
            (prefix / 'capture-build.json').write_text(manifest.replace('false', 'true'))
            with self.assertRaises(ValueError):
                module.copy_notices(prefix, root / 'wrong', 'test-hash')
            self.assertFalse((root / 'wrong').exists())
