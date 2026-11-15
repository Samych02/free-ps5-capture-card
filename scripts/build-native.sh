#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG=Debug
TESTS=OFF
while (($#)); do
    case "$1" in
        --help) echo "Usage: scripts/build-native.sh [--config Debug|Release] [--tests]"; exit 0 ;;
        --config)
            if (($# < 2)); then echo "--config requires a value" >&2; exit 2; fi
            CONFIG="$2"; shift 2 ;;
        --tests) TESTS=ON; shift ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
case "$CONFIG" in Debug|Release) ;; *) echo "Invalid configuration: $CONFIG" >&2; exit 2 ;; esac
BUILD_DIR="$ROOT/build/native/$CONFIG"
for tool in cmake ninja brew pkg-config; do
    command -v "$tool" >/dev/null || { echo "Missing $tool; run scripts/bootstrap.sh" >&2; exit 1; }
done
PYTHON="$ROOT/build/python/bin/python3"
if [[ ! -x "$PYTHON" ]]; then echo "Missing isolated Python; run scripts/bootstrap.sh" >&2; exit 1; fi
OPENSSL="$(brew --prefix openssl@3)"
PROTOBUF="$(brew --prefix protobuf@29)"
FFMPEG="$(python3 "$ROOT/scripts/build-ffmpeg.py")"
FFMPEG_FIXTURES=""
if [[ $TESTS == ON ]]; then FFMPEG_FIXTURES="$(python3 "$ROOT/scripts/build-ffmpeg.py" --fixtures)"; fi
set -x
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CONFIG" -DBUILD_TESTING="$TESTS" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 \
    -DOPENSSL_ROOT_DIR="$OPENSSL" -DCMAKE_PREFIX_PATH="$OPENSSL;$PROTOBUF" \
    -DCAPTURE_FFMPEG_ROOT="$FFMPEG" -DCAPTURE_FFMPEG_FIXTURE_ROOT="$FFMPEG_FIXTURES" \
    -DPROTOC="$PROTOBUF/bin/protoc" -DPYTHON_EXECUTABLE="$PYTHON"
cmake --build "$BUILD_DIR" --parallel
