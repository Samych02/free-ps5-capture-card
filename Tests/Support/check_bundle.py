#!/usr/bin/env python3
"""Validate a developer app's recursive runtime closure (not release approval)."""
import argparse
import sys
import plistlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from bundle_runtime import binaries, dependencies, rpaths, resolve, run, system

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', required=True)
    app = Path(parser.parse_args().app).resolve()
    if not (app / 'Contents/Frameworks/CaptureEngine.framework').exists():
        raise RuntimeError('CaptureEngine.framework is not embedded')
    source_notices = app / 'Contents/Resources/ProjectNotices/EmbeddedSources'
    for component in ('chiaki', 'gf-complete', 'jerasure', 'nanopb', 'curl'):
        if not (source_notices / component).is_dir():
            raise RuntimeError(f'Missing embedded-source license notices: {component}')
    if (app / 'Contents/PlugIns').exists():
        raise RuntimeError('Developer bundle must not contain test plug-ins')
    with (app / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    minimum = tuple(map(int, info['LSMinimumSystemVersion'].split('.')))
    code = binaries(app)
    if not code:
        raise RuntimeError('No Mach-O code found')
    for binary in code:
        if 'arm64' not in run('lipo', '-archs', str(binary)).split():
            raise RuntimeError(f'No arm64 slice: {binary.name}')
        import re
        versions = re.findall(r'\bminos\s+(\d+(?:\.\d+)*)', run('vtool', '-show-build', str(binary)))
        for version in versions:
            if tuple(map(int, version.split('.'))) > minimum:
                raise RuntimeError(f'{binary.name} requires macOS {version}, app declares {info["LSMinimumSystemVersion"]}')
        paths = rpaths(binary)
        if any(p.startswith('/') and not system(p) for p in paths):
            raise RuntimeError(f'External runtime search path: {binary.name}')
        for dependency in dependencies(binary):
            if system(dependency):
                continue
            if dependency.startswith('/'):
                raise RuntimeError(f'Absolute non-system dependency: {dependency}')
            target = resolve(dependency, binary, app, paths)
            if not target.is_relative_to(app) or not target.exists():
                raise RuntimeError(f'Non-bundled dependency: {dependency}')
            if re.search(r'(Qt[0-9]?|SDL|chiaki-ng)', dependency, re.I):
                raise RuntimeError(f'Forbidden runtime GUI dependency: {dependency}')
    run('codesign', '--verify', '--deep', '--strict', str(app))
    print(f'PASS: {len(code)} Mach-O files, arm64, recursive runtime closure, minimum OS and signatures.')
    print('Clean-machine launch, PS5/OBS evidence and independent license/security review remain release gates.')
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, OSError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
