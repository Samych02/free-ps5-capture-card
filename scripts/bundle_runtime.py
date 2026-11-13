#!/usr/bin/env python3
"""Assemble an ad-hoc developer runtime closure. Never installs or publishes."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

MAGICS = {b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'}

def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)

def binaries(app):
    found = set()
    for path in app.rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open('rb') as stream:
            if stream.read(4) in MAGICS:
                found.add(path.resolve())
    return sorted(found)

def dependencies(path):
    return [line.strip().split(' (compatibility version', 1)[0] for line in run('otool', '-L', str(path)).splitlines()[1:] if line.strip()]

def rpaths(path):
    return re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', run('otool', '-l', str(path)))

def system(path):
    return path.startswith(('/usr/lib/', '/System/Library/'))

def expand(value, owner, app):
    return Path(value.replace('@loader_path', str(owner.parent)).replace('@executable_path', str(app / 'Contents/MacOS')))

def resolve(value, owner, app, paths):
    if value.startswith('@rpath/'):
        for base in paths + ['@executable_path/../Frameworks']:
            candidate = expand(base, owner, app) / value[len('@rpath/'):]
            if candidate.exists():
                return candidate.resolve()
        raise RuntimeError(f'Unresolved runtime dependency {value} in {owner.name}')
    return expand(value, owner, app).resolve()

def bundle(app):
    app = app.resolve()
    destination = app / 'Contents/Frameworks'
    notices = app / 'Contents/Resources/RuntimeNotices'
    destination.mkdir(exist_ok=True)
    notices.mkdir(parents=True, exist_ok=True)
    pending = binaries(app)
    seen, copies, formulas = set(), {}, {}
    manifest = []
    while pending:
        owner = pending.pop(0)
        if owner in seen:
            continue
        seen.add(owner)
        paths = rpaths(owner)
        for dependency in dependencies(owner):
            if system(dependency):
                continue
            source = resolve(dependency, owner, app, paths)
            if source == owner:
                continue  # dylib's own install ID
            if source.is_relative_to(app):
                pending.append(source)
                continue
            if not source.is_file():
                raise RuntimeError(f'Missing library {source}')
            name = source.name
            if name in copies and copies[name] != source:
                raise RuntimeError(f'Conflicting runtime library names: {name}')
            target = destination / name
            if name not in copies:
                if target.exists():
                    raise RuntimeError(f'Refusing to replace existing bundle file: {target}')
                copies[name] = source
                shutil.copy2(source, target)
                target.chmod(target.stat().st_mode | 0o200)
                run('install_name_tool', '-id', '@rpath/' + name, str(target))
                pending.append(target.resolve())
                manifest.append({'library': name, 'source': str(source)})
                parts = source.parts
                if 'Cellar' in parts:
                    index = parts.index('Cellar')
                    formula = Path(*parts[:index + 3])
                    formulas[str(formula)] = formula
            run('install_name_tool', '-change', dependency, '@rpath/' + name, str(owner))
        # Runtime search paths must not retain build/user/Homebrew locations.
        for path in paths:
            if path.startswith('/') and not system(path):
                run('install_name_tool', '-delete_rpath', path, str(owner))
    for formula in formulas.values():
        output = notices / (formula.parent.name + '-' + formula.name)
        output.mkdir(exist_ok=True)
        licenses = [p for p in formula.iterdir() if p.is_file() and re.match(r'(COPYING|COPYRIGHT|LICENSE|NOTICE)', p.name, re.I)]
        if not licenses:
            raise RuntimeError(f'No top-level license notice found for {formula.parent.name}; review before bundling')
        for path in licenses:
            shutil.copy2(path, output / path.name)
        for path in (formula / '.brew').glob('*.rb'):
            shutil.copy2(path, output / path.name)
    (notices / 'runtime-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    # Sign code inside-out only after every install-name edit has completed.
    for binary in binaries(app):
        run('codesign', '--force', '--sign', '-', str(binary))
    for framework in sorted((app / 'Contents/Frameworks').glob('*.framework')):
        run('codesign', '--force', '--sign', '-', str(framework))
    run('codesign', '--force', '--sign', '-', str(app))
    print(f'Bundled {len(copies)} non-system libraries; ad-hoc developer signing only.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    bundle(parser.parse_args().app)
