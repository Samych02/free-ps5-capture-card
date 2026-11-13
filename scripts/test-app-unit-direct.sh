#!/usr/bin/env bash
# Explicit unit-only alternative when the IDE/XCTest session cannot initialize.
# It does not exercise application launch or UI automation and is never a fallback.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/scripts/build.sh" --config Debug
cd "$ROOT"
xcodebuild -project FreePS5CaptureCard.xcodeproj -scheme FreePS5CaptureCard \
    -configuration Debug -derivedDataPath build/xcode \
    -destination 'platform=macOS,arch=arm64' build-for-testing \
    -only-testing:FreePS5CaptureCardTests
python3 - <<'PY'
import os
from pathlib import Path
import subprocess
root = Path.cwd()
app = root / 'build/xcode/Build/Products/Debug/FreePS5CaptureCard.app'
runner = '/Applications/Xcode.app/Contents/Developer/usr/bin/xctest'
environment = {
    'HOME': os.environ['HOME'],
    'TMPDIR': os.environ['TMPDIR'],
    'PATH': '/usr/bin:/bin',
    'DYLD_FRAMEWORK_PATH': str(root / 'build/native/frameworks') + ':/Applications/Xcode.app/Contents/SharedFrameworks',
    'DYLD_INSERT_LIBRARIES': str(app / 'Contents/MacOS/FreePS5CaptureCard.debug.dylib'),
}
result = subprocess.run([runner, str(app / 'Contents/PlugIns/FreePS5CaptureCardTests.xctest')],
                        env=environment, capture_output=True, text=True, timeout=120)
output = result.stdout + result.stderr
print(output)
if result.returncode or 'Executed 0 tests' in output or 'Test Suite' not in output:
    raise SystemExit(result.returncode or 1)
PY
