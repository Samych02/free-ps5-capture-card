# Capture-only negotiation — author verification

## Scope and limits

Developer build 4 explicitly enables a new default-false Chiaki core policy.
This supersedes the earlier negotiation deferral; it is not independent review
or a release clearance. No PS5 is available. Whether the console accepts the
omitted messages and continues A/V is **unverified**; earlier device evidence
does not validate this candidate. No silent legacy fallback exists.

Locked patch 0004 suppresses feedback startup/state emission, controller-connected
announcement, microphone STREAMINFO/connect/toggle, and keyboard/DualSense enable
requests. Launch JSON contains an empty connectedControllers array instead of the
legacy xinput/ds3/ds4 advertisement. Controller state calls reject before mutation;
mic calls reject before output. Cleanup checks active state, failed starts never
finalize uninitialized
feedback, and repeated start/stop are safe. Legacy zero-initialized callers retain
input setup and ordering. Core layouts change: rebuild consumers. App C API stays v3.

Media ACKs, stream heartbeat, display restrictions, IDR and compressed two-track
transport are not removed. No preview, playback, input features, decoding/encoding,
credential changes, additional platform builds or OS-minimum changes are included.

## Test seams and fresh evidence

- Red tests first: policy build failed for missing fields/helpers; separate Native
  lifecycle test failed at `SessionTests.cpp:235` before app opt-in. Logs:
  `build/capture-input-policy-red.log`, `build/capture-only-app-optin-red.log`.
- Final source review caught the separate hard-coded launch controller list.
  `build/capture-only-launch-red.log` records the missing launch policy field;
  `build/capture-only-launch-green.log` records the repaired build and focused
  regression pass. BIG's real setup path passes the policy to a test interceptor
  that calls the real formatter, checks capture-only/legacy JSON and media width/
  height, then returns failure before ECDH/network. Successful BIG exchange and
  console acceptance are not simulated.
- `CapturePolicyStream.c` includes real patched streamconnection.c and exposes
  internal helpers with test-only Takion/feedback substitutions. Tests decode the
  actual generated protobufs. Real ctrl.c sends encrypted frames to an AF_UNIX
  socket pair using synthetic keys. No console connection or real secrets.
- Final focused `ctest --test-dir build/native/Debug --no-tests=error -R
  '^capture-(input-policy|session|media-sink)$' --output-on-failure`: 3/3 pass.
  Tests cover capture-only suppression, legacy default, explicit input rejection,
  repeated lifecycle calls, failed feedback start/first input send and retained
  stream ACK/heartbeat. They are not an end-to-end console negotiation simulator.
- ASan/UBSan recompilation of policy tests, stream wrapper, actual session.c,
  ctrl.c and launchspec.c passes. Compile/link flags were taken from Ninja's
  existing policy target,
  adding `-fsanitize=address,undefined -fno-omit-frame-pointer` and placing those
  five instrumented objects before the existing archive dependencies. Remaining
  archive/dependency code is uninstrumented; no whole-core sanitizer audit claim.
  Log: `build/capture-policy-sanitized/verification.log`.
- All four patches apply sequentially to a pristine archive of the pinned revision;
  the six policy sources match Debug byte-for-byte afterward. Cached patch
  validation passes twice in both Debug and Release. Initial verification harness
  accidentally let Git discover the parent repo and skip paths; initializing the
  scratch repo fixed the harness before the successful comparison.

## Stable checkpoint and artifact

- `python3 -m unittest discover -s Tests/Bootstrap -v`: **27/27 pass**.
- `ctest --test-dir build/native/Debug --no-tests=error --output-on-failure`:
  **13/13 targets pass**, 187.82 s, including transport and final launch policy.
- `bash scripts/test-app-unit-direct.sh`: **92/92 pass**, no failures. This is
  unit/off-screen evidence, not interactive UI acceptance.
- `scripts/package-dev-app.sh --output build/package-capture-only`: Release
  **0.1.0 build 4**, **15 Mach-O checks pass**, 13 bundled dylibs, arm64/macOS 26,
  recursive runtime closure and ad-hoc signatures. Embedded lock and notices
  match the current source files. Logs: `build/capture-only-{bootstrap,native,app,package}-regression.log`.
- App: `build/package-capture-only/Free PS5 Capture Card.app`.
  ZIP: `build/FreePS5CaptureCard-0.1.0-dev4-capture-only-arm64.zip`, **8,544,364 bytes**,
  CRC passes; SHA-256
  `b6f43ab90e4031794ad9fcf2d7c9142a9c19c691b06df564f5edeabdd154a69e`.
  The three earlier ZIP hashes are unchanged.

No publishing, Applications installation, autostart or notarization is authorized
or performed.

## Author review and remaining gates

Scoped author source review covers flag initialization/copy, all input send sites,
feedback locking/start/failure/cleanup, unchanged media send paths, app opt-in and
patch provenance. This is not independent code/security/license review.
Real-device negotiation, continuing A/V, Stop/reconnect/PIN/HDR transitions,
calibrated sync/latency/soak, clean-machine launch, interactive accessibility,
independent review and complete corresponding-source delivery remain open.
Hosted CI and receiver-reset integration also remain unverified as previously
documented. Nothing here clears public redistribution.
