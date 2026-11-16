# Capture-only controller/microphone policy

Living Execution Plan under PLANS.md. Baseline `a8b19c4`, main. User explicitly
chose **enable capture-only in a new developer candidate**, superseding the prior
deferral. No real console is available; do not claim negotiation compatibility.

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
