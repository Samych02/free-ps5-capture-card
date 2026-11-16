# Capture-only negotiation — author verification

## Build-5 initial-control-request follow-up

User now reports build 4 works on the PS5, but the Remote Play user shows keyboard,
controller and microphone icons. Executable path verified build 4 was running.
This is basic user-reported operation, not calibrated/both-track/soak evidence.
Build-4 offline tests missed the initial HTTP request: it still declared
`RP-ControllerType: 3` and `RP-PadProcNo: 3`. That missed signaling boundary is
proven; whether it causes the icons is not. No undocumented client-type changes
or icon-removal promise are justified.

User additionally reports FIFA shows two controllers/two player slots while
build 4 is connected. This suggests a real extra controller-slot effect, not only
decorative Remote Play icons; the exact cause is still unproven. At user request,
build 4 was gracefully quit and verified stopped before opening build 5, avoiding
concurrent capture app instances. Build-5 FIFA/controller-slot retest is pending.

Locked patch 0005 extracts the real request formatter and omits those two headers
only in capture-only mode. Legacy PS5 request is checked byte-for-byte. Client
identity, authentication, encryption counters, streaming/bitrate fields, transport
and media handling are preserved. `CapturePolicyCtrl.c` compiles real ctrl.c with
normal writes and exposes only the internal formatter using synthetic strings.
No live request, authentication material or credential file was inspected/logged.

- Behavior red: `build/capture-handshake-behavior-red.log` fails at the actual
  request's RP-ControllerType absence assertion before the policy guard.
- Green: `build/capture-handshake-green.log` passes capture-only omission, legacy
  exact request, PS5/older-PS4 paths, retained media fields and truncation rejection.
- `ctest --test-dir build/native/Debug --no-tests=error -E transport --output-on-failure`:
  **12/12 pass**, 12.07 s. Full unchanged transport was last run for build 4 below;
  it was not repeated for this request-format-only follow-up.
- Focused ASan/UBSan tests, real ctrl/stream wrappers, session.c and launchspec.c
  pass; remaining dependencies uninstrumented. Log:
  `build/capture-handshake-sanitized/verification.log`.
- All five locked patches apply to pristine pinned source; ctrl.c matches Debug
  byte-for-byte. Twice-cached validation succeeds in Debug and Release.
- Bootstrap: **27/27 pass** (`build/capture-handshake-bootstrap.log`).
- Direct app unit runner: **92/92 pass** (`build/capture-handshake-app.log`).
- Release package: **15 Mach-O checks pass**, 13 bundled dylibs, arm64/macOS 26,
  ad-hoc signatures/recursive closure (`build/capture-handshake-package.log`).
  Embedded notices/lock match current source. These three checks took 20.73 s.
- App: `build/package-capture-handshake/Free PS5 Capture Card.app`, **0.1.0 build 5**.
  ZIP: `build/FreePS5CaptureCard-0.1.0-dev5-capture-handshake-arm64.zip`, **8,544,817
  bytes**, CRC passes; SHA-256
  `19feccefc704bd95fe176d9314401d5b0f9e4a387a09ff943156a517937d74e3`.
- Scoped author review covered actual call-site use, request argument order and
  bounds, legacy byte equivalence, unchanged crypto-counter steps/media/client
  identity, and five-patch provenance. It is not independent review.
- Running build 4 was not stopped/replaced. Build-5 device compatibility and icon
  behavior remain unverified. Other manual/independent/source-delivery gates remain.

The following sections preserve the build-4 checkpoint and its evidence.

## Build-4 scope and limits

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
