# Capture-only negotiation — author verification

## Build-8 microphone-disconnected experiment

Device result: user reports **"not gone but it still works"**. Thus build 8
continues operating by user observation, but the mic indicator remains; the
explicit connected=false declaration is not sufficient to achieve visible mic
removal in this test. This does not establish whether the PS5 processed/ignored
the message, whether controller_id=0 identifies the UI device, or whether the
indicator reflects an actual selectable input. No calibrated A/V evidence is
implied. Stop stacking payload guesses: next inspect PS5 Settings > Sound >
Microphone > Input Device while connected, including listed/selected device, to
distinguish an advertised Remote Play input from a local controller mic or UI-only
indicator. Keep current operation intact; no further protocol change yet.

User prioritized mic removal after reporting PS5 controller/mic indicators still
present in build 7, and approved an explicit disconnected-state experiment rather
than muting or keeping complete mic-message suppression. The pinned takion.proto
defines MICCONNECTION (30), but the pinned core has no other call site: field
existence is evidence of a schema, not evidence the PS5 honors it or hides icons.

Revised patch 0004 encodes controller_id=0, connected=false, absent result in
capture-only input setup, using the existing reliable Takion channel-1 path.
The slot identifier is an explicit experimental assumption, not an established
mapping. Encoding/send failure propagates through the existing setup-failure path;
there is no enable/mute fallback, mic audio, controller feedback, HTTP/client-type
change or extra app input feature. Build-7 handshake and patch 0005 are unchanged.

- Red: `build/capture-mic-disconnected-red.log` fails on the absent MICCONNECTION.
- Green tests decode real protobuf output and assert exact type/required fields,
  connected=false, absent result, channel/
  flags, no controller/mic enable setup, and send-failure propagation. Legacy
  output, feedback lifecycle/control guards, stream ACK and heartbeat remain tested.
- All five locked patches apply to pristine pinned source; ctrl.c and
  streamconnection.c match current sources. Cached validation twice per Debug/
  Release succeeds. **12/12 non-transport native targets pass**, 12.31 s
  (`build/capture-mic-disconnected-native.log`); unchanged transport not rerun.
- Focused ASan/UBSan policy tests and real stream/control/session/launchspec pass;
  remaining dependencies uninstrumented
  (`build/capture-mic-disconnected-sanitized/verification.log`).
- Bootstrap **27/27**, direct app unit tests **92/92**, packaged **15 Mach-O checks**
  pass, with 13 dylibs, arm64/macOS 26 and ad-hoc signatures/recursive closure.
  These three stages took 19.94 s. Logs:
  `build/capture-mic-disconnected-{bootstrap,app,package}.log`.
- App: `build/package-mic-disconnected/Free PS5 Capture Card.app`, **0.1.0 build 8**.
  Version and embedded notices/lock match source. Only patch 0004's digest changes
  from build 7; all digests verify, including unchanged handshake patch 0005.
- Scoped author review covers required false encoding/controller identifier,
  reliable channel, bounded buffer, unchanged legacy/media/controller-handshake
  behavior and propagating send errors without mic enable/fallback. This is not
  independent review and does not establish PS5 semantics.
- No capture app was running at handoff; build 8 was opened without stopping a
  session or auto-starting capture. Prior candidates remain intact. Do not call
  mic removal fixed: device acceptance must check connection, OBS game video/audio
  and PS5 mic indicator/input-device listing separately.

## Build-7 one-header experiment

Latest user observation: build 7 opens and PS5 controller/microphone indicators
still appear. User now prioritizes removing the microphone indication. Do not
interpret this as calibrated media acceptance or a passed FIFA slot test. Current
capture-only code sends no mic audio, mic STREAMINFO/connect/toggle, but that
omission has not removed the PS5 indicator. The pinned takion.proto defines
MICCONNECTION (30) with required controller_id and connected bool; no pinned core
call site uses it. A connected=false message is a possible explicit negative-state
experiment, not established PS5 behavior. Muting is not equivalent to disconnecting.
Leave controller HTTP headers alone; no new mic-enabled capability or raw logs.

The user confirmed build 4 works again after build-6 CTRL_CONNECT_FAILED. This
strongly implicates the combined initial-header omission in builds 5/6, not which
header is needed or whether either controls icons/FIFA slots. Build 7 retains
RP-ControllerType: 3 and omits only RP-PadProcNo in capture-only mode. Relative to
working build 4, this is the only negotiation difference. Fixed safe diagnostic
categories from build 6 remain; feedback/mic traffic and launch controller-list
suppression stay. No automatic retry-to-another-policy or fallback exists.

This deliberately retains one controller declaration for a controlled experiment,
not a claim of absent controller-slot allocation or a solution to the user's FIFA
symptom. Working build 4 is left running. The fifth patch/digest is revised;
prior commits/packages preserve the exact earlier source. Do not add an overlapping
sixth override patch: cached validation reverse-checks earlier patches individually.

- Red: `build/capture-controller-header-red.log` fails on missing ControllerType.
- Capture-only generated HTTP is now verified byte-for-byte equal to the legacy
  request with exactly the PadProcNo line removed. Legacy remains exact, request
  bounds/paths/media fields and feedback/mic suppression remain covered.
- Pristine five-patch application matches ctrl.c; cached validation twice in Debug
  and Release passes. Non-transport native targets: **12/12 pass**, 12.37 s
  (`build/capture-controller-header-native.log`). Full transport was not repeated.
- Focused ASan/UBSan real ctrl/stream/session/launchspec and policy tests pass;
  remaining dependencies uninstrumented
  (`build/capture-controller-header-sanitized/verification.log`).
- Bootstrap: **27/27 pass**, app unit tests: **92/92 pass**, package: **15 Mach-O
  checks pass**, 13 dylibs, arm64/macOS 26, ad-hoc signatures/recursive closure.
  Logs: `build/capture-controller-header-{bootstrap,app,package}.log`; those
  three stages took 19.72 s. App: `build/package-controller-header/Free PS5 Capture Card.app`,
  **0.1.0 build 7**. Embedded lock/notices and every patch digest match source.
- Scoped author review confirms the sole negotiation delta from build 4 is
  PadProcNo omission; ControllerType, client identity, crypto order, media and
  capture-only input suppression are unchanged. This is not independent review.
- No app was stopped/switched during build work. At handoff process inspection
  found no capture app running (cause unknown); build 7 was then opened without
  starting capture. Device connection and icon/FIFA-slot results remain pending.
  Do not call this candidate fixed or erase the working build-4 comparison.

## Build-6 safe quit-category diagnostics

User supplied build 5's Recent events message, but it repeated the same generic
"PS5 connection ended" text. Inspection proves `classifyQuit` maps eight distinct
typed transient core categories to that sentence; the reason is lost at our
presentation layer, not available through another UI disclosure. Protocol-failure
root cause remains unknown. Build 6 preserves build 5's five-patch negotiation
and only replaces those cases with fixed local allowlisted messages/tags for
session request, control connection/handshake and media connection endings.
No remote strings, request bytes, credential reads or raw logging are introduced;
retry decisions and authentication/input/media behavior are unchanged.

Regression `build/capture-quit-category-red.log` fails because the category tag
is absent. Native lifecycle coverage also injects a synthetic private remote
reason and verifies only the fixed category reaches status.

Build 6 at `build/package-capture-diagnostics/Free PS5 Capture Card.app` passes:
12/12 non-transport native targets (10.08 s), 27 Bootstrap checks, 92 app tests,
and 15 Mach-O package checks with 13 dylibs. Build/test/package took 34.1 s total;
logs: `build/capture-quit-category-{native-build,native,bootstrap,app,package}.log`.
Version verified 0.1.0 build 6. Embedded dependency lock exactly matches build 5;
no new upstream patch, negotiation or crypto/media/input change. Full transport
was not rerun for this message-only change. Scoped author review confirms fixed
strings, unchanged retry classification and ignored remote reason payloads.
Do not label this diagnostic candidate a fix; next device evidence must identify
its displayed safe category before further protocol edits. App was not silently
switched; the running retry history is preserved for the user to control.

Device follow-up: build 6 reports **CTRL_CONNECT_FAILED**. This establishes
failure in ctrl_connect/control setup, before media streaming; that category
also includes transport/request/response failures, so it does not by itself
prove header rejection or which header is required. No further protocol edits
were made. Build 6 was gracefully quit and verified stopped, then preserved
build 4 was explicitly opened for a controlled comparison (not a silent fallback).
Build 4 has the original two HTTP headers but the same no-feedback/no-mic policy.
User must start capture; current-condition baseline result is pending.

User confirmed the reopened build 4 connects/works again under the comparison
conditions. Working build 4 retains both controller HTTP headers; builds 5/6
omit both and build 6 reports CTRL_CONNECT_FAILED. This strongly implicates the
combined header-removal change, but does not identify which header is required
or whether either controls PS5/FIFA slot allocation. Keep working build 4 intact.
Next experiment should vary one header only, with feedback/mic suppression held
constant; no further changes to client type or multiple fields together.

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

Build-5 device follow-up: user screenshot shows **Reconnecting to PS5… / PS5
connection ended. Retrying; check power and local network access.** Process path
confirms build 5 is running. This is a failed/unstable connection observation,
not a successful icon/controller-slot test. Missing controller headers are a
regression suspect, not an established cause; transient console state is not
excluded. Need the initiating Recent events reason before Stop clears it, then a
controlled comparison with build 4. Do not stack further protocol guesses or
claim the device issue fixed from offline tests.

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
