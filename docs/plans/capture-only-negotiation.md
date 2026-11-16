# Capture-only controller/microphone policy

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
