#!/usr/bin/env python3
"""Fetch/assemble/verify matching release source locally. Never publishes or installs.

An intact inventory is engineering evidence, not independent license review or
proof of a bit-identical build. No live credentials or working-tree files are
exported: project source comes only from an explicit committed revision.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
from urllib.parse import urlsplit
from urllib.request import urlopen
import uuid

MAX_DOWNLOAD = 512 * 1024 * 1024
MACHO_MAGICS = {b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'}


def safe_relative(value):
    if (not isinstance(value, str) or not value or '\\' in value or
            any(ord(c) < 32 for c in value) or value.startswith('/')):
        raise ValueError('Unsafe source path')
    path = PurePosixPath(value)
    if not path.parts or any(p in ('.', '..') for p in value.rstrip('/').split('/')):
        raise ValueError('Unsafe source path: ' + value)
    return Path(*path.parts)


def expect_hash(value, length=64):
    if not isinstance(value, str) or not re.fullmatch('[0-9a-f]{' + str(length) + '}', value):
        raise ValueError('Invalid pinned digest/revision')


def secure_url(value):
    parts = urlsplit(value)
    if (parts.scheme != 'https' or not parts.hostname or parts.username is not None or
            parts.password is not None or parts.fragment or any(ord(c) < 32 for c in value)):
        raise ValueError('Source URL must be HTTPS without credentials or fragments')


def validate_asset(asset):
    if safe_relative(asset['path']).as_posix() != asset['path']:
        raise ValueError('Source asset path must be canonical')
    secure_url(asset['url'])
    expect_hash(asset['sha256'])


def assets(manifest):
    return [asset for component in manifest['components'] for asset in component['assets']]


def validate_manifest(manifest):
    if manifest.get('schema_version') != 1 or not manifest.get('components'):
        raise ValueError('Unsupported or empty release-source manifest')
    expect_hash(manifest['project_revision'], 40)
    expect_hash(manifest['dependency_lock_sha256'])
    expect_hash(manifest['app_tree_sha256'])
    for key in ('app_build', 'app_version'):
        if not re.fullmatch(r'[0-9][0-9.]*', manifest[key]):
            raise ValueError('Invalid app identity')
    ids, paths, libraries = set(), set(), set()
    for component in manifest['components']:
        name = component['id']
        if not re.fullmatch(r'[a-z0-9][a-z0-9@+_.-]*', name) or name in ids:
            raise ValueError('Invalid or duplicate component')
        ids.add(name)
        if component['kind'] not in ('homebrew', 'ffmpeg') or not component['assets']:
            raise ValueError('Unknown component kind or missing source assets')
        if safe_relative(component['version']).name != component['version']:
            raise ValueError('Invalid component version path')
        if component['kind'] == 'homebrew':
            expect_hash(component['recipe_sha256'])
            if not component.get('libraries'):
                raise ValueError('Homebrew component has no runtime coverage')
            for library in component['libraries']:
                if safe_relative(library).name != library or library.casefold() in libraries:
                    raise ValueError('Invalid or duplicate runtime library')
                libraries.add(library.casefold())
        elif name != 'ffmpeg' or len(component['assets']) != 1:
            raise ValueError('Expected one FFmpeg source archive')
        prefix = 'homebrew/' + name + '/' if component['kind'] == 'homebrew' else 'ffmpeg/'
        for asset in component['assets']:
            validate_asset(asset)
            if not asset['path'].startswith(prefix) or asset['path'].casefold() in paths:
                raise ValueError('Incorrect or duplicate source asset path')
            paths.add(asset['path'].casefold())
    if 'ffmpeg' not in ids:
        raise ValueError('Missing static FFmpeg source')


def sha256(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def regular_file(path, expected=None):
    if path.is_symlink() or not path.is_file():
        raise ValueError('Missing or linked source file: ' + str(path))
    if expected is not None and sha256(path) != expected:
        raise ValueError('Source checksum mismatch: ' + str(path))
    return path


def managed_path(root, relative):
    if root.is_symlink():
        raise ValueError('Managed source root must not be a symlink')
    path = root
    for part in safe_relative(relative).parts:
        path = path / part
        if path.is_symlink():
            raise ValueError('Symlink inside managed source tree: ' + str(path))
    return path


def verified_cache_file(cache, asset):
    validate_asset(asset)
    return regular_file(managed_path(cache, asset['sha256']), asset['sha256'])


def fetch_one(asset, cache, opener=urlopen):
    validate_asset(asset)
    if cache.is_symlink():
        raise ValueError('Source cache must not be a symlink')
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / asset['sha256']
    if target.exists() or target.is_symlink():
        return verified_cache_file(cache, asset)
    temporary = cache / ('.' + uuid.uuid4().hex + '.part')
    try:
        with opener(asset['url'], timeout=60) as response, temporary.open('xb') as output:
            secure_url(response.url)
            total = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > MAX_DOWNLOAD:
                    raise ValueError('Source archive exceeds download bound')
                output.write(chunk)
        regular_file(temporary, asset['sha256'])
        try:
            os.link(temporary, target)  # Atomic no-overwrite cache insertion.
        except FileExistsError:
            verified_cache_file(cache, asset)
        return verified_cache_file(cache, asset)
    finally:
        temporary.unlink(missing_ok=True)


def read_app_file(app, relative):
    return regular_file(managed_path(app, relative)).read_bytes()


def app_inventory(app):
    if app.is_symlink() or not app.is_dir():
        raise ValueError('Invalid frozen app directory')
    records = []
    for path in sorted(app.rglob('*')):
        name = path.relative_to(app).as_posix()
        safe_relative(name)
        if path.is_symlink():
            if not path.resolve().is_relative_to(app.resolve()):
                raise ValueError('App contains an external link')
            records.append({'path': name, 'symlink': str(path.readlink())})
        elif path.is_file():
            records.append({'path': name, 'bytes': path.stat().st_size,
                            'mode': path.stat().st_mode & 0o777, 'sha256': sha256(path)})
        elif not path.is_dir():
            raise ValueError('App contains a special file')
    return records


def app_tree_digest(app):
    return hashlib.sha256(json.dumps(app_inventory(app), sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def validate_app(app, manifest):
    validate_manifest(manifest)
    info = plistlib.loads(read_app_file(app, 'Contents/Info.plist'))
    if (str(info.get('CFBundleVersion')) != manifest['app_build'] or
            info.get('CFBundleShortVersionString') != manifest['app_version']):
        raise ValueError('App version/build does not match release source')
    project = 'Contents/Resources/ProjectNotices/EmbeddedSources/'
    lock = read_app_file(app, project + 'Dependencies.lock.json')
    if hashlib.sha256(lock).hexdigest() != manifest['dependency_lock_sha256']:
        raise ValueError('App dependency lock mismatch')
    expected = set()
    for component in manifest['components']:
        if component['kind'] == 'homebrew':
            recipe = ('Contents/Resources/RuntimeNotices/' + component['id'] + '-' +
                      component['version'] + '/' + component['id'] + '.rb')
            if hashlib.sha256(read_app_file(app, recipe)).hexdigest() != component['recipe_sha256']:
                raise ValueError('Embedded formula recipe mismatch: ' + component['id'])
            expected.update((library, component['id'], component['version']) for library in component['libraries'])
        else:
            ffmpeg = json.loads(read_app_file(app, project + 'ffmpeg/capture-build.json'))
            if (ffmpeg.get('fixtures') is not False or
                    ffmpeg.get('source', {}).get('sha256') != component['assets'][0]['sha256']):
                raise ValueError('Static FFmpeg source/profile mismatch')
    actual = set()
    runtime = json.loads(read_app_file(app, 'Contents/Resources/RuntimeNotices/runtime-manifest.json'))
    for item in runtime:
        library = item['library']
        if safe_relative(library).name != library:
            raise ValueError('Invalid runtime library name')
        parts = Path(item['source']).parts
        try:
            index = parts.index('Cellar')
            name, version = parts[index + 1:index + 3]
        except (ValueError, IndexError):
            raise ValueError('Runtime origin is not a known Homebrew component')
        row = (library, name, version)
        if row in actual or Path(item['source']).name != library:
            raise ValueError('Duplicate or mismatched runtime origin')
        actual.add(row)
    if not expected or actual != expected:
        raise ValueError('Runtime library source coverage is incomplete or mismatched')
    executable = info.get('CFBundleExecutable', '')
    if not executable or safe_relative(executable).name != executable:
        raise ValueError('Invalid app executable identity')
    expected_code = {'Contents/MacOS/' + executable,
                     'Contents/Frameworks/CaptureEngine.framework/Versions/A/CaptureEngine'}
    expected_code.update('Contents/Frameworks/' + library for library, _, _ in expected)
    code = set()
    for path in app.rglob('*'):
        if path.is_symlink():
            if not path.resolve().is_relative_to(app.resolve()):
                raise ValueError('App contains an external link')
        elif path.is_file():
            with path.open('rb') as stream:
                if stream.read(4) in MACHO_MAGICS:
                    code.add(path.relative_to(app).as_posix())
    if code != expected_code:
        raise ValueError('Missing or unlisted Mach-O source coverage')
    if app_tree_digest(app) != manifest['app_tree_sha256']:
        raise ValueError('App file/link/mode inventory differs from the frozen candidate')
    return runtime


def archive_project(repo, revision, output):
    expect_hash(revision, 40)
    archive = subprocess.check_output(['git', 'archive', '--format=tar', revision], cwd=repo)
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        members = source.getmembers()
        for member in members:
            safe_relative(member.name)
            if not (member.isfile() or member.isdir()):
                raise ValueError('Project export contains a link or special entry')
        output.mkdir(parents=True, exist_ok=False)
        for member in members:
            path = managed_path(output, member.name)
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as stream, path.open('xb') as target:
                    shutil.copyfileobj(stream, target)
                path.chmod(member.mode & 0o777)


def write_git_bundle(source, expected, output):
    expect_hash(expected, 40)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    shallow = subprocess.check_output(['git', 'rev-parse', '--is-shallow-repository'], cwd=source, text=True).strip()
    if head != expected or shallow != 'false':
        raise ValueError('Git source revision differs or history is incomplete: ' + str(source))
    if output.exists() or output.is_symlink():
        raise FileExistsError(str(output))
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'bundle', 'create', str(output.resolve()), 'HEAD'], cwd=source, check=True)


def tree_files(directory):
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Invalid source tree')
    result = {}
    for path in sorted(directory.rglob('*')):
        relative = path.relative_to(directory).as_posix()
        safe_relative(relative)
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise ValueError('Linked or special source payload: ' + relative)
        if path.is_file() and relative != 'SHA256SUMS':
            result[relative] = sha256(path)
    return result


def seal_tree(directory):
    entries = tree_files(directory)
    (directory / 'SHA256SUMS').write_text(''.join(digest + '  ' + name + '\n' for name, digest in entries.items()))


def verify_tree(directory):
    checksum_file = regular_file(managed_path(directory, 'SHA256SUMS'))
    expected = {}
    for line in checksum_file.read_text().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        if not match:
            raise ValueError('Malformed source checksum list')
        digest, name = match.groups()
        safe_relative(name)
        if name in expected or name == 'SHA256SUMS':
            raise ValueError('Duplicate or self-referential source checksum')
        expected[name] = digest
    if not expected or tree_files(directory) != expected:
        raise ValueError('Source payload is missing, changed, extra or unverified')
    return len(expected)


def copy_regular_tree(source, output):
    entries = tree_files(source)
    output.mkdir(parents=True, exist_ok=False)
    for name in entries:
        target = managed_path(output, name)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, target)


def validate_recipe(component, text):
    # This payload is macOS-only. Resources outside explicit Linux-only blocks
    # need an implemented source-input entry, never a silent recipe-only omission.
    macos_text = re.sub(r'^  on_linux do\n.*?^  end$', '', text, flags=re.M | re.S)
    if re.search(r'''^\s*resource(?:\s+|\(\s*)["']''', macos_text, re.M):
        raise ValueError('Uncovered macOS formula resource: ' + component['id'])
    source_hash = re.search(r'^  sha256 "([0-9a-f]{64})"', text, re.M)
    if not source_hash or source_hash.group(1) != component['assets'][0]['sha256']:
        raise ValueError('Formula stable source differs from pinned archive: ' + component['id'])
    blocks = re.findall(r'^  patch do\n(.*?)^  end$', text, re.M | re.S)
    directives = re.findall(r'^\s*patch\b[^\n]*', text, re.M)
    if len(blocks) != len(directives) or len(blocks) != len(component['assets']) - 1:
        raise ValueError('Incomplete formula patch coverage: ' + component['id'])
    for block, asset in zip(blocks, component['assets'][1:]):
        file = re.search(r'file "([^"]+)"', block)
        url = re.search(r'url "([^"]+)"', block)
        digest = re.search(r'sha256 "([0-9a-f]{64})"', block)
        if file:
            if Path(file.group(1)).name != Path(asset['path']).name:
                raise ValueError('Local formula patch order/path mismatch')
        elif not url or not digest or url.group(1) != asset['url'] or digest.group(1) != asset['sha256']:
            raise ValueError('Remote formula patch order/hash mismatch')


def assemble(manifest, cache, app, repo, core, instructions, output):
    runtime = validate_app(app, manifest)
    instructions = regular_file(instructions)
    for asset in assets(manifest):
        verified_cache_file(cache, asset)
    revision = manifest['project_revision']
    lock_bytes = subprocess.check_output(['git', 'show', revision + ':Dependencies.lock.json'], cwd=repo)
    if hashlib.sha256(lock_bytes).hexdigest() != manifest['dependency_lock_sha256']:
        raise ValueError('Project snapshot dependency lock differs from app')
    lock = json.loads(lock_bytes)
    for component in manifest['components']:
        if component['kind'] == 'homebrew':
            path = (app / 'Contents/Resources/RuntimeNotices' /
                    (component['id'] + '-' + component['version']) / (component['id'] + '.rb'))
            validate_recipe(component, path.read_text())
    if output.exists() or output.is_symlink():
        raise FileExistsError(str(output))
    output.mkdir(parents=True)
    archive_project(repo, revision, output / 'project')
    git_sources = []
    repositories = [('', core, lock['chiaki']['revision'])]
    for submodule in lock['chiaki']['submodules']:
        safe_relative(submodule)
        record = subprocess.check_output(['git', 'ls-tree', 'HEAD', '--', submodule], cwd=core, text=True).split()
        if len(record) < 3 or record[0] != '160000':
            raise ValueError('Missing pinned submodule: ' + submodule)
        repositories.append((submodule, core / submodule, record[2]))
    for submodule, source, expected in repositories:
        name = submodule.replace('/', '-') if submodule else 'chiaki'
        relative = 'git/' + name + '.bundle'
        write_git_bundle(source, expected, output / relative)
        git_sources.append({'path': relative, 'checkout': submodule or '.', 'revision': expected})
    for asset in assets(manifest):
        target = managed_path(output / 'upstream', asset['path'])
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(verified_cache_file(cache, asset), target)
    copy_regular_tree(app / 'Contents/Resources/RuntimeNotices', output / 'notices/runtime')
    copy_regular_tree(app / 'Contents/Resources/ProjectNotices', output / 'notices/project')
    (output / 'source-inputs.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (output / 'app-inventory.json').write_text(json.dumps({
        'version': manifest['app_version'], 'build': manifest['app_build'],
        'source_revision': revision, 'app_tree_sha256': manifest['app_tree_sha256'],
        'files': app_inventory(app),
        'scope': 'Frozen file bytes, symlink targets and file modes; not an independent build attestation.',
    }, indent=2) + '\n')
    (output / 'source-inventory.json').write_text(json.dumps({
        'schema_version': 1, 'project_revision': revision, 'app_build': manifest['app_build'],
        'git_sources': git_sources, 'runtime_libraries': runtime,
        'release_status': 'engineering assembly only; independent/source-build/manual gates not certified',
        'excluded_resources': 'OpenSSL Linux-only Perl test resources are not used by this macOS build.',
    }, indent=2) + '\n')
    shutil.copyfile(instructions, output / 'REBUILD.md')
    (output / 'tools').mkdir()
    shutil.copyfile(Path(__file__), output / 'tools/prepare-release.py')
    seal_tree(output)
    return verify_tree(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    fetch = sub.add_parser('fetch', help='Explicitly fetch SHA-256-pinned HTTPS source inputs')
    fetch.add_argument('--manifest', type=Path, required=True)
    fetch.add_argument('--cache', type=Path, required=True)
    build = sub.add_parser('assemble', help='Offline assembly from committed source and verified cache')
    for name in ('manifest', 'cache', 'app', 'repo', 'core', 'instructions', 'output'):
        build.add_argument('--' + name, type=Path, required=True)
    verify = sub.add_parser('verify', help='Verify all source-tree payload hashes; no legal-clearance claim')
    verify.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args()
    if args.action == 'verify':
        print('Verified source payload files:', verify_tree(args.directory))
        return
    manifest = json.loads(args.manifest.read_text())
    validate_manifest(manifest)
    if args.action == 'fetch':
        with ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(lambda asset: fetch_one(asset, args.cache), assets(manifest)))
        print('Verified cached source inputs:', len(assets(manifest)))
    else:
        count = assemble(manifest, args.cache, args.app, args.repo, args.core, args.instructions, args.output)
        print('Assembled and verified source payload files:', count)
        print('No release published, assets uploaded, or license/clean-machine gates certified.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        print('Release source preparation failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
