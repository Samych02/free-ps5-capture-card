#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE=0; APP=0; UI=0; FILTER=""; ONLY=""
while (($#)); do
    case "$1" in
        --native) NATIVE=1; shift ;;
        --app) APP=1; shift ;;
        --ui) UI=1; shift ;;
        --filter|--only)
            if (($# < 2)); then echo "$1 requires a value" >&2; exit 2; fi
            if [[ $1 == --filter ]]; then FILTER="$2"; else ONLY="$2"; fi
            shift 2 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
if (( !NATIVE && !APP && !UI )); then echo "Select --native, --app and/or --ui" >&2; exit 2; fi
case "$FILTER" in ""|interface|pairing|media-sink|timeline|queue|codec-units|session|diagnostics|mux|tcp) ;; *) echo "Unknown native suite: $FILTER" >&2; exit 2 ;; esac
if [[ -n $FILTER && $NATIVE != 1 ]]; then echo "--filter requires --native" >&2; exit 2; fi
if [[ -n $ONLY && $APP != 1 ]]; then echo "--only requires --app" >&2; exit 2; fi
cd "$ROOT"
if ((NATIVE)); then
    "$ROOT/scripts/build-native.sh" --config Debug --tests
    if [[ $FILTER == interface || $FILTER == pairing || $FILTER == media-sink || $FILTER == timeline || $FILTER == queue || $FILTER == codec-units || $FILTER == session || $FILTER == diagnostics ]]; then
        ctest --test-dir build/native --no-tests=error -R "^capture-$FILTER$" --output-on-failure
    elif [[ $FILTER == mux || $FILTER == tcp ]]; then
        case_name=remux-equivalence
        if [[ $FILTER == tcp ]]; then case_name=tcp; fi
        "$ROOT/build/python/bin/python3" Tests/Support/integration.py --build-dir build/native --case "$case_name"
    else
        ctest --test-dir build/native --no-tests=error --output-on-failure
    fi
fi
if ((APP || UI)); then
    "$ROOT/scripts/build.sh" --config Debug
    args=(-project FreePS5CaptureCard.xcodeproj -scheme FreePS5CaptureCard
        -configuration Debug -derivedDataPath build/xcode
         -destination 'platform=macOS,arch=arm64' test)
    if (( !UI )) && [[ -z $ONLY ]]; then args+=("-only-testing:FreePS5CaptureCardTests"); fi
    if (( !APP )); then args+=("-only-testing:FreePS5CaptureCardUITests"); fi
    if [[ -n $ONLY ]]; then
        IFS=',' read -r -a classes <<< "$ONLY"
        for name in "${classes[@]}"; do
            case "$name" in AppModelTests|EngineClientTests|OptionsTests|AuthTests|CredentialStoreTests|AccountModelTests|PairingTests) ;; *) echo "Unknown app test class: $name" >&2; exit 2 ;; esac
            args+=("-only-testing:FreePS5CaptureCardTests/$name")
        done
    fi
    set -x
    xcodebuild "${args[@]}"
fi
