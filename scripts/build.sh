#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG=Debug
if (($#)); then
    if [[ $# != 2 || $1 != --config ]]; then echo "Usage: scripts/build.sh [--config Debug|Release]" >&2; exit 2; fi
    CONFIG="$2"
fi
case "$CONFIG" in Debug|Release) ;; *) echo "Invalid configuration: $CONFIG" >&2; exit 2 ;; esac
"$ROOT/scripts/build-native.sh" --config "$CONFIG" --tests
cd "$ROOT"
set -x
xcodegen generate --spec project.yml
xcodebuild -project FreePS5CaptureCard.xcodeproj -scheme FreePS5CaptureCard \
    -configuration "$CONFIG" -derivedDataPath build/xcode \
    -destination 'platform=macOS,arch=arm64' build
