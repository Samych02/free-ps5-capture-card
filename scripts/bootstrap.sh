#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if (($#)); then echo "Usage: scripts/bootstrap.sh" >&2; exit 2; fi
for tool in brew cmake ninja xcodegen ffmpeg ffprobe pkg-config; do
    command -v "$tool" >/dev/null || { echo "Missing $tool. Install the documented Homebrew prerequisites first." >&2; exit 1; }
done
for formula in openssl@3 protobuf@29 json-c miniupnpc libevent python@3.13; do
    brew --prefix "$formula" >/dev/null
done
PYTHON="$(brew --prefix python@3.13)/bin/python3.13"
if [[ ! -x "$ROOT/build/python/bin/python3" ]]; then
    "$PYTHON" -m venv "$ROOT/build/python"
fi
"$ROOT/build/python/bin/python3" -m pip install --disable-pip-version-check 'protobuf==5.29.6'
echo "Isolated build Python ready. Run scripts/build-native.sh --config Debug --tests"
