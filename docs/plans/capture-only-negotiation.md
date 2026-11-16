# Capture-only controller/microphone policy

## Current microphone-priority investigation

Build-8 device result: user reports mic indicator **not gone**, but operation
still works. Do not call the negative-state experiment successful mic removal or
claim the PS5 processed the message. Pause further speculative protocol builds.
Next evidence: PS5 Settings > Sound > Microphone > Input Device listing and
selection while connected, distinguishing a Remote Play input from the local
controller mic/static indicator. No new payload/client-type/id changes yet.

**Approved goal:** test an explicit microphone-disconnected declaration, not mute
or mic audio, without changing the working build-7 controller/media handshake.
**Architecture:** in capture-only stream input setup, encode one schema-defined
MICCONNECTION with controller_id=0, connected=false and no result field. Slot 0 is
the experiment's explicit identifier, not proven PS5 binding. Retain the guarded
legacy path, real control-entrypoint rejection and zero feedback/mic-audio setup.
Send on Takion reliable message channel 1 using the existing input-setup path;
encoding/send errors propagate through that path, never fallback/mute/enable.
**Constraints:** user approved negative-state signaling as an exception to prior
complete mic-message suppression. Keep build 7/its package; no HTTP/client-type,
credential/logging, preview, app input or media ACK/heartbeat/restriction changes.
**Acceptance:** offline exact protobuf and failure tests pass, legacy unchanged;
device icon removal/session A/V acceptance remains a separate user test.

### Task 1: Negative-state payload regression and implementation

- [x] Update `Tests/Native/CapturePolicyTests.cpp` to assert exactly MICCONNECTION,
  controller_id=0, connected=false, absent result; no STREAMINFO/controller-connect
  or feedback. Preserve control guards and media ACK/heartbeat checks. Add forced
  send-failure coverage. Run `cmake --build build/native/Debug --target
  capture-input-policy-tests --parallel` and the test executable: fail on absent
  negative-state message before implementation.
- [x] Update the capture-only branch in locked patch 0004's real stream input
  setup, revise its digest in `Dependencies.lock.json`, synchronize fetched Debug/
  Release and advance `project.yml` to build 8. Do not change patch 0005/HTTP setup.
  Re-run the policy executable: exact payload and error checks pass.

### Task 2: Offline checkpoint and separate device candidate

- [x] Verify pristine locked patch application and cached validation twice in
  both configurations; regenerate focused ASan/UBSan policy executable using
  Ninja compile/link flags (stream/ctrl wrappers, session/launchspec and tests).
- [x] Run `scripts/build-native.sh --config Debug --tests`, then
  `ctest --test-dir build/native/Debug --no-tests=error -E transport --output-on-failure`,
  Bootstrap unittest discovery and `bash scripts/test-app-unit-direct.sh`. Expect
  no failures; do not re-run unchanged multi-minute transport for this small test.
- [x] Update notices/evidence and package via `scripts/package-dev-app.sh --output
  build/package-mic-disconnected`; verify build 8, embedded lock/notices and binary
  checks. Commit scoped files on main, preserving the unrelated deleted doc.
- [x] Hand off as unverified mic-disconnect experiment, not a fix or guaranteed
  absent mic icon; no automatic switch of an active working capture session.

Build 8 at `build/package-mic-disconnected/Free PS5 Capture Card.app`: native
12/12 non-transport targets, Bootstrap 27/27, app 92/92, focused policy sanitizers
and 15 packaged Mach-O checks pass. No capture app was running at handoff, so
build 8 was opened without stopping a session or auto-starting capture. User must
verify connection, OBS video/game audio and mic-indicator/device-list behavior.

Design/plan author review: Approved for bounded implementation. Message fields,
slot assumption, ordering, send errors, no-input boundaries, build provenance and
missing console evidence are explicit. User approved the direction in-chat; per
repo instruction continue without another milestone approval or worktree.

User reports build 7 still shows PS5 controller/mic indicators and asks to remove
at least the mic. Mic audio and enable/connect/toggle signaling are already absent;
do not claim icon removal from that fact. Pinned takion.proto defines unused
MICCONNECTION with controller_id and connected bool. Proposed next design is an
isolated explicit connected=false message in a separate candidate, holding the
working controller headers/media/authentication fixed. This changes the prior
"no mic signaling" policy to allow only an explicit negative-state declaration;
user approved that experiment, then test serialized payload/failure paths
before device handoff. No support for mic input, no mute-only substitution and no
claim that an unused schema field establishes actual PS5 acceptance/icon behavior.

## Current build-7 one-header experiment

The controlled return to build 4 works; builds 5/6 omit both HTTP controller
headers and build 6 fails CTRL_CONNECT_FAILED. This implicates the combined
omission, without establishing which header matters or that icons/extra FIFA
slot are controlled by either. Keep the working build 4 running and unchanged.

Build 7 explicitly retains RP-ControllerType: 3 and omits only RP-PadProcNo in
capture-only mode. Feedback/mic traffic remains suppressed; safe build-6 category
messages stay. Relative to working build 4, the only negotiation difference is
the missing PadProcNo header. It is an isolated experiment, not a proposed fix,
silent fallback, or claim of absent PS5 controller-slot allocation. Revise the
fifth locked patch/digest; earlier commits/artifacts retain build-5/6 provenance.

- [x] Red test proves controller header is absent before this change.
- [x] Retain controller header and assert capture-only HTTP equals legacy HTTP
  with exactly the PadProcNo line removed. All other suppression tests remain.
- [x] Pristine/cached validation, focused native/app/Bootstrap and sanitizer checks.
- [x] Package separately, document evidence and checkpoint. Do not switch the
  healthy running build 4 automatically; next device result is build-7 connection
  and, only if connected, PS5 icons/FIFA slot behavior.

Build 7 ready at `build/package-controller-header/Free PS5 Capture Card.app`:
12 non-transport native targets, 27 Bootstrap checks, 92 app tests, focused
ASan/UBSan and 15 packaged Mach-O checks pass. At completion a process check found
no capture app running (do not infer why); build work did not stop/switch build 4.
Build 7 was then opened for the one-header test, without auto-starting capture.
Await user connection and controller-slot observations; do not claim a fix.

## Current build-6 diagnostic follow-up

Build 5 shows Reconnecting to PS5 on the user's console, and Recent events repeats
the same generic connection-ended sentence. Root cause is not established.
`classifyQuit` collapses session-request, control and media failures into that
sentence despite receiving a typed core quit reason. Build 6 exposes only fixed
local allowlisted labels for those eight existing categories; retry decisions,
negotiation, locked patches, credentials and raw logging remain unchanged.

- [x] Red diagnostics regression proves distinct reasons were lost.
- [x] Add fixed safe messages, verify diagnostics/session/native/app checks and
  build separate candidate with unchanged five-patch negotiation. Never append
  remote/core strings or enable raw request/hexdump logging.
- [x] Hand off diagnostic candidate, without claiming header removal is fixed
  or replacing the running app without authorization. Next device evidence is
  the safe quit category, then one controlled comparison—not more protocol guesses.

Build 6: `build/package-capture-diagnostics/Free PS5 Capture Card.app`. Native
non-transport 12/12, Bootstrap 27/27, app 92/92 and package 15 Mach-O checks pass;
all build/verification stages took 34.1 s. Package lock is identical to build 5.
Full transport was not rerun for this fixed-message-only change. Device reason
and recovery are pending; no protocol fix or icon/controller-slot fix claimed.

User reports build-6 CTRL_CONNECT_FAILED: control setup fails before media, but
the category does not distinguish TCP/request/response or identify a required
header. Build 4 was explicitly reopened as a controlled baseline comparison,
after build 6 exited; capture was not auto-started. Wait for this baseline result
before changing individual headers or undocumented client types. Do not mistake
disabled input traffic for evidence of absent PS5 controller-slot allocation.

User confirmed the reopened build 4 works. The A/B result strongly implicates
omitting the two handshake headers together (builds 5/6 fail), without identifying
which is required. Preserve build 4 as the working candidate. Next bounded
experiment must change only one header while keeping feedback/mic suppression
and all other negotiation fixed; controller-slot/icon outcome remains unresolved.

Living Execution Plan under PLANS.md. Baseline `a8b19c4`, main. User explicitly
chose **enable capture-only in a new developer candidate**, superseding the prior
deferral. No console was available at design time; see the later device observation
and build-5 follow-up below. Do not claim unperformed negotiation acceptance.

## Build-5 handshake follow-up

User now reports build 4 works on the PS5 but its Remote Play user shows keyboard,
controller and microphone icons. Running executable path confirmed build 4, not
a stale candidate. This is user-reported basic connection evidence only; explicit
both-track/HDR/soak acceptance is not established. The initial HTTP control request
still declares RP-ControllerType: 3 and PS5 RP-PadProcNo: 3; patch 0004 missed that
boundary. These declarations are proven, their causal relationship to all three
icons is not. Do not promise icon removal or invent undocumented client types.

- [x] Add real request formatter regression; show an actual assertion failure
  while the original declarations remain.
- [x] Guard both controller headers behind the existing capture_only policy in
  a fifth locked patch; preserve legacy request byte-for-byte and client identity,
  auth/encryption counters, bitrate/codec selection and transport behavior.
- [x] Verify request bounds/paths, input suppression, media/native/app regressions,
  focused sanitizers, pristine five-patch and cached validation.
- [x] Package separate build 5, document evidence and checkpoint. Do not interrupt
  the running build-4 capture or silently switch the user's app.

Use focused native checks (excluding the unchanged multi-minute transport suite)
for this request-format-only follow-up; disclose that full transport was last run
at build 4. Keep both prior developer candidates and all unrelated user changes.

Build 5 is ready at `build/package-capture-handshake/Free PS5 Capture Card.app`.
27 Bootstrap checks, 92 app tests, 12 non-transport native targets, focused
ASan/UBSan and 15 packaged Mach-O checks pass. Separate ZIP/hash and detailed
scope are in the policy report. Do not claim the PS5 icons fixed: next device
check is whether build 5 connects with both tracks and changes the icons.

## Progress

- [x] Trace feedback lifecycle, controller announcement, microphone signaling,
  feature enable and public input entrypoints. Present design/activation tradeoff;
  user approved enabling the candidate, with device acceptance pending.
- [x] Add red native policy/output/lifecycle tests and app opt-in regression.
- [x] Implement new locked upstream patch and enable it in Native Session.
- [x] Verify capture-only suppression, legacy behavior, failure/cleanup paths,
  pristine/cached patch application and focused sanitizer checks.
- [x] Run stable regressions, assemble separate developer candidate and document
  limitations/review/checkpoint without new platform or release claims.

## Design and execution

Append a default-false `capture_only` field to ChiakiConnectInfo and copy it into
session-owned connection data. Native/Session.cpp explicitly sets true. The app's
own C API stays v3 (no public capture_status/options change); upstream internal
struct layouts change, so rebuild all users of the pinned core.

Capture-only suppresses feedback sender creation/state emission, connected-
controller announcements (including the launch JSON's connectedControllers list),
microphone STREAMINFO and mute/unmute/connect messages.
It overrides keyboard/DualSense feature enable requests and rejects explicit
controller/microphone API calls before mutating state or emitting messages.
Do not acquire a Mac microphone/controller, decode/transcode media, change media
ACKs/heartbeats/IDR/restrictions, or add a GUI toggle/hidden fallback.

Extract the existing feedback start/stop and controller/microphone setup into
small internal functions called by the actual run/stream-info paths. Lifecycle
cleanup checks whether feedback was really started; capture-only, failed start
and repeated cleanup must not finalize uninitialized feedback state. Legacy
default false retains original input setup, ordering, and error propagation.

Tests compile real streamconnection.c through a wrapper with test-only symbol
substitution for Takion sends and feedback sender operations. Real ctrl.c is
linked normally; actual encrypted control frames are observed over an AF_UNIX
socket pair (synthetic keys only), without substituting writes.
The real BIG path's launch formatter is intercepted after calling the real
formatter and inspecting policy/JSON; it then returns failure before ECDH/network
setup. This tests policy propagation and serialized controller lists in both modes,
not successful BIG exchange or console acceptance.
Record emitted control/protobuf message types and lifecycle counts, not payloads
or secrets. Session initialization uses synthetic keys/numeric loopback hosts;
no console/network connection is started. Test capture-only and legacy modes,
forced input calls, send/start failures, repeated stop and media-preserving paths.
Keep existing media integration regressions and prove Native explicitly opts in.

Changes belong in a fourth digest-locked Chiaki patch, not only ignored sources.
Use sequential patch application/reverse checks carefully: prior patches already
modify session/streamconnection. Verify clean application and repeated cached
configure, both configurations. Reuse pinned FFmpeg and native caches; do not
repeat the full transport suite during each edit. At the stable checkpoint run
Bootstrap/native/app tests, focused policy sanitizers and a new developer package.

## Safety and acceptance

Stay on main with scoped local checkpoint(s); preserve unrelated deletion of
docs/plans/file-credential-storage.md and all earlier packages. No edits to the
external chiaki-ng checkout, publication, Applications install or notarization.
Keep Apple Silicon/macOS 26 and native SwiftUI. Build number 4 distinguishes this
unverified negotiation candidate from last-known-working negotiation build 3.

Automated acceptance is no forbidden input messages/feedback in capture-only,
legacy default unchanged, safe cleanup and unchanged two-track media tests.
Device acceptance is explicitly pending: whether the PS5 accepts absent input
announcements, continues A/V and handles reconnect/Stop/HDR/PIN. If not, investigate
the minimum necessary negotiation later; never silently fall back to input mode.

## Surprises & Discoveries

- Feedback startup and shutdown are currently unconditional; suppressing startup
  alone would finalize uninitialized feedback state. Gate both through active state.
- No existing upstream capture-only flag exists. `enable_dualsense=false` does not
  disable ordinary controller feedback/announcements or microphone signaling.
- Final source review found a second controller advertisement: launchspec.c's
  hard-coded connectedControllers list. Added a red propagation/format regression,
  then carried the policy into ChiakiLaunchSpec and emitted [] in capture-only.
  Legacy formatted list is unchanged; launch-spec layout also requires rebuilding.

## Decision Log

- User chose enabled developer candidate rather than disabled preparation only.
- Preserve legacy default for other core callers; no global deletion of upstream
  input implementation. Guard policy at actual sends and explicit entrypoints.
- Read-only design readiness pass: Approved; scope, test seams, activation and
  missing device evidence are explicit. No separate agent/independent review.

## Outcomes & Retrospective

Completed offline: 27 Bootstrap checks, 13 native targets, 92 app unit tests,
focused policy ASan/UBSan and 15 packaged Mach-O checks pass. Separate build-4 app:
`build/package-capture-only/Free PS5 Capture Card.app`; ZIP/evidence in
`docs/reviews/capture-only-negotiation.md`. Previous archives are unchanged.
No actual-device, independent-review or public-distribution gate has passed.
Next when hardware is available: verify absent input announcements are accepted,
both media tracks reach OBS, then Stop/start and supported game/HDR transitions.
