#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG=Release
OUTPUT="$ROOT/build/package"
while (($#)); do
    case "$1" in
        --config|--output)
            if (($# < 2)); then echo "$1 requires a value" >&2; exit 2; fi
            if [[ $1 == --config ]]; then CONFIG="$2"; else OUTPUT="$2"; fi
            shift 2 ;;
        *) echo "Usage: scripts/package-dev-app.sh [--config Debug|Release] [--output DIRECTORY]" >&2; exit 2 ;;
    esac
done
case "$CONFIG" in Debug|Release) ;; *) echo "Invalid configuration" >&2; exit 2 ;; esac
cd "$ROOT"
APP="$OUTPUT/Free PS5 Capture Card.app"
if [[ -e "$APP" ]]; then echo "Output app already exists; choose a new output directory (nothing was overwritten)." >&2; exit 2; fi
"$ROOT/scripts/build.sh" --config "$CONFIG"
mkdir -p "$OUTPUT"
ditto "$ROOT/build/xcode/Build/Products/$CONFIG/FreePS5CaptureCard.app" "$APP"
mkdir -p "$APP/Contents/Resources/ProjectNotices"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$APP/Contents/Resources/ProjectNotices/"
ditto "$ROOT/LICENSES" "$APP/Contents/Resources/ProjectNotices/LICENSES"
python3 - "$ROOT" "$APP" "$CONFIG" <<'PY'
from pathlib import Path
import re
import shutil
import sys
root, app = map(Path, sys.argv[1:3])
configuration = sys.argv[3]
source = root / 'build/native' / configuration / '_deps/chiaki-src'
destination = app / 'Contents/Resources/ProjectNotices/EmbeddedSources'
for name, directory in [('chiaki', source)] + [(name, source / 'third-party' / name) for name in ('gf-complete', 'jerasure', 'nanopb')]:
    output = destination / name
    output.mkdir(parents=True)
    notices = [p for p in directory.iterdir() if p.is_file() and re.match(r'(COPYING|COPYRIGHT|LICENSE|NOTICE|AUTHORS)', p.name, re.I)]
    if not notices:
        raise SystemExit(f'Missing source license notices: {name}')
    for path in notices:
        shutil.copy2(path, output / path.name)
    if (directory / 'LICENSES').is_dir():
        shutil.copytree(directory / 'LICENSES', output / 'LICENSES')
shutil.copy2(root / 'Dependencies.lock.json', destination / 'Dependencies.lock.json')
PY
cmake -DAPP="$APP" -DROOT="$ROOT" -P "$ROOT/cmake/BundleRuntime.cmake"
python3 "$ROOT/Tests/Support/check_bundle.py" --app "$APP"
