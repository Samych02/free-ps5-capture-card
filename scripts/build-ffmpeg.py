#!/usr/bin/env python3
"""Build a pinned private FFmpeg prefix; stdout is only the resulting path.

Current execution adapter is arm64 macOS 26. Profiles/source verification are
platform-independent; new targets need explicit toolchain adapters and evidence.
Never installs globally or falls back to the system FFmpeg libraries.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[1]


def profile_flags(fixtures=False):
    flags = ['--disable-everything', '--disable-autodetect', '--disable-programs',
             '--disable-doc', '--disable-debug', '--disable-network',
             '--disable-avdevice', '--disable-avfilter', '--disable-swscale',
             '--disable-swresample', '--disable-shared', '--enable-static',
             '--enable-pic', '--disable-iconv', '--disable-zlib', '--disable-bzlib',
             '--disable-lzma', '--disable-videotoolbox', '--disable-audiotoolbox',
             '--disable-securetransport', '--enable-parser=h264,hevc',
             '--enable-muxer=matroska']
    if fixtures:
        flags += ['--enable-demuxer=matroska', '--enable-protocol=file',
                  '--enable-bsf=h264_mp4toannexb,hevc_mp4toannexb']
    return flags


def build_identity(source_hash, flags, target, toolchain):
    return hashlib.sha256(json.dumps([source_hash, flags, target, toolchain],
                                    sort_keys=True).encode()).hexdigest()[:20]


def verified_extract(archive, destination, expected):
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise ValueError('FFmpeg source checksum mismatch; refusing to extract or execute')
    with tarfile.open(archive) as source:
        members = source.getmembers()
        for member in members:
            path = Path(member.name)
            if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir()):
                raise ValueError('Unsafe FFmpeg source archive entry')
        destination.mkdir(parents=True)
        # macOS's system Python predates tarfile filters. The complete preflight
        # above allows only relative files/directories (no links or devices),
        # and the destination is newly created, never an existing source tree.
        source.extractall(destination, members=members)


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def build(fixtures=False):
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        raise RuntimeError('Only Apple Silicon/macOS 26 builds are supported today; add a verified toolchain adapter for a new target')
    pin = json.loads((ROOT / 'Dependencies.lock.json').read_text())['ffmpeg']
    compiler = output('xcrun', '--find', 'clang')
    sdk = output('xcrun', '--sdk', 'macosx', '--show-sdk-path')
    toolchain = [compiler, output(compiler, '--version'), sdk,
                 output('xcrun', '--sdk', 'macosx', '--show-sdk-version')]
    flags = profile_flags(fixtures) + ['--arch=arm64', '--target-os=darwin',
             '--cc=' + compiler, '--sysroot=' + sdk,
             '--extra-cflags=-mmacosx-version-min=26.0 -fvisibility=hidden',
             '--extra-ldflags=-mmacosx-version-min=26.0']
    # Include this recipe's bytes so behavior changes cannot reuse stale builds.
    identity = build_identity(pin['sha256'], flags, 'darwin-arm64-macos26',
                              toolchain + [hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
    directory = ROOT / 'build/dependencies/ffmpeg' / identity
    prefix = directory / 'install'
    stamp = prefix / 'capture-build.json'
    if stamp.is_file() and all((prefix / 'lib' / f'lib{name}.a').is_file()
                               for name in ('avformat', 'avcodec', 'avutil')):
        return prefix
    directory.mkdir(parents=True, exist_ok=True)
    # A second process may build another profile, but never the same prefix.
    lock = directory / '.building'
    try:
        fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError:
        raise RuntimeError(f'FFmpeg build already active or interrupted: {lock}; check the owner process before removing the lock')
    try:
        with os.fdopen(fd, 'w') as stream:
            stream.write(str(os.getpid()))
        archive = ROOT / 'build' / ('ffmpeg-' + pin['version'] + '.tar.xz')
        if not archive.exists():
            temporary = archive.with_name(archive.name + '.' + uuid.uuid4().hex + '.part')
            try:
                if not pin['url'].startswith('https://'):
                    raise ValueError('FFmpeg source URL must use HTTPS')
                with urllib.request.urlopen(pin['url'], timeout=60) as response, temporary.open('xb') as target:
                    if not response.url.startswith('https://'):
                        raise ValueError('Refusing non-HTTPS source redirect')
                    shutil.copyfileobj(response, target)
                if hashlib.sha256(temporary.read_bytes()).hexdigest() != pin['sha256']:
                    raise ValueError('Downloaded FFmpeg source checksum mismatch')
                os.replace(temporary, archive)
            finally:
                temporary.unlink(missing_ok=True)
        # Revalidate the archive even when a previous attempt extracted it.
        if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']:
            raise ValueError('Cached FFmpeg source checksum mismatch')
        sources = directory / 'source'
        if not sources.exists():
            verified_extract(archive, sources, pin['sha256'])
        source = sources / ('ffmpeg-' + pin['version'])
        objects = directory / 'objects'
        objects.mkdir(exist_ok=True)
        environment = os.environ.copy()
        for name in ('CFLAGS', 'CPPFLAGS', 'LDFLAGS', 'PKG_CONFIG_PATH', 'CPATH', 'LIBRARY_PATH'):
            environment.pop(name, None)
        environment['SDKROOT'] = sdk
        commands = [[str(source / 'configure'), '--prefix=' + str(prefix)] + flags,
                    ['make', '-j' + str(min(os.cpu_count() or 2, 12))], ['make', 'install']]
        with (directory / 'build.log').open('w') as log:
            for command in commands:
                print('FFmpeg ' + ('fixtures' if fixtures else 'runtime') + ': ' + command[0], file=sys.stderr)
                subprocess.run(command, cwd=objects, env=environment, stdout=log,
                               stderr=subprocess.STDOUT, check=True)
        notices = prefix / 'share/capture-ffmpeg'
        notices.mkdir(parents=True, exist_ok=True)
        for name in ('LICENSE.md', 'COPYING.LGPLv2.1', 'COPYING.LGPLv3', 'COPYING.GPLv2', 'COPYING.GPLv3'):
            shutil.copy2(source / name, notices / name)
        manifest = {'identity': identity, 'source': pin, 'fixtures': fixtures,
                    'target': 'darwin-arm64-macos26', 'configure': commands[0], 'toolchain': toolchain}
        stamp.write_text(json.dumps(manifest, indent=2) + '\n')
        return prefix
    finally:
        lock.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', action='store_true', help='Separate test-only Matroska/file/Annex-B input profile')
    arguments = parser.parse_args()
    try:
        print(build(arguments.fixtures))
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'FFmpeg build failed: {error}', file=sys.stderr)
        sys.exit(1)
