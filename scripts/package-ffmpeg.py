#!/usr/bin/env python3
"""Preserve static minimal-FFmpeg notices/provenance in a developer bundle."""
import argparse
import json
from pathlib import Path
import re
import shutil


def copy_notices(prefix, destination, expected_hash):
    manifest = prefix / 'capture-build.json'
    data = json.loads(manifest.read_text())
    if data.get('fixtures') is not False or data.get('source', {}).get('sha256') != expected_hash:
        raise ValueError('Refusing wrong-source or fixture-only FFmpeg in an app bundle')
    source = prefix / 'share/capture-ffmpeg'
    for name in ('LICENSE.md', 'COPYING.LGPLv2.1'):
        if not (source / name).is_file():
            raise ValueError('Missing complete FFmpeg license: ' + name)
    shutil.copytree(source, destination)
    shutil.copy2(manifest, destination / manifest.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    args = parser.parse_args()
    cache = (args.root / 'build/native' / args.config / 'CMakeCache.txt').read_text()
    match = re.search(r'^CAPTURE_FFMPEG_ROOT:[^=\n]*=(.+)$', cache, re.M)
    if not match:
        raise ValueError('Missing explicit FFmpeg prefix in native CMake cache')
    pin = json.loads((args.root / 'Dependencies.lock.json').read_text())['ffmpeg']
    destination = args.app / 'Contents/Resources/ProjectNotices/EmbeddedSources/ffmpeg'
    copy_notices(Path(match[1]), destination, pin['sha256'])
    shutil.copy2(args.root / 'scripts/build-ffmpeg.py', destination / 'build-ffmpeg.py')


if __name__ == '__main__':
    main()
