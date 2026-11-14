# First-release UI and status polish Execution Plan

## Goal and approved direction

Prepare a native single-window 0.1.0 release candidate, not a dashboard redesign.
User explicitly chose native utility polish and requests an audit of all screens
and glitches. Work on main with local checkpoints; preserve the unrelated deleted
`docs/plans/file-credential-storage.md`. No publication, notarization, Applications
installation, auto-start, secret logging, media decoding/encoding or hidden HDR
fallback. No claim of eliminating every possible bug or clearing untested gates.

## Progress

- [x] Inspect every view, account/pairing/capture models and native status paths.
- [x] User approved native single-window direction and release-candidate scope.
- [x] Structured phases and separate source/output history, C API v2 and Swift
  decoding; native targets and model tests pass.
- [x] Capture/input/discovery/passcode lifecycle fixes with focused regressions.
- [x] Native layout polish across main window and all setup/passcode sheets.
- [x] Actual SwiftUI light/dark/narrow screenshots and UI-test compilation:
  30 PNGs in `build/ui-polish`; edge cases and all setup sheets included.
- [x] Focused automated verification, package, review handoff and release checklist.
- [ ] Independent review and the remaining hands-on/distribution gates in
  `docs/release-checklist.md`; no public-release-ready claim.

## Design and file responsibilities

Capture remains the main action. Header and current-health panel precede setup,
stream options and OBS connection instructions. Start/Stop stay easy to find;
disabled Start explains prerequisites. Use native system type, semantic colours,
clear labels, generous but compact spacing, a bounded readable width, no preview.
All errors wrap and remain accessible without colour alone. Native field labels,
keyboard default/cancel actions and focus should be consistent across sheets.

`Native/include/CaptureEngine.h`, `Pipeline.cpp`, `Engine.cpp`: add a typed output
phase and separate bounded historical output/source messages. API version bumps
for the changed status layout; all bundled clients rebuild together. Current
messages contain current problems only. Blocked/paused cannot report Streaming.
Retain reset diagnostics without showing them as live failures. Transient retry
snapshots should not flash terminal failure. Stop/new capture clear stale status.

`App/EngineClient.swift`: decode new fields and native snapshot state. Main-thread
presentation must use one snapshot rather than combine an older callback state
with newer status. Preserve protocol test injection and no secret-bearing logs.

`App/Models/CapturePresentation.swift`: pure, exhaustively tested presentation of
idle, prerequisites, connecting, media wait, OBS wait, output start, live, paused,
protected, PIN, retrying, stopping and failure. History is not current health.

`App/AppModel.swift`: explicit stopping/submitting feedback, guard duplicate
actions, require valid raw port text, keep setup locked until Stop acknowledgement,
clear passcode submission state on native updates. Do not retain PINs. Reject
invalid port text rather than silently starting with the previous parsed value.

`App/PairingModel.swift`: owned cancellable discovery progress, separate discovery
errors from pairing errors, reject stale results, reset progress on retry/cancel.
`PairingRequest` exposes the same strict eight-ASCII-digit validator used by UI.

`App/Views`: extract focused reusable status, metrics, stream-options and passcode
views. Fix disclosure alignment with explicit leading/full-width content; use
readable metric rows and clarify counter drift is not A/V sync. Keep all metric
and historical details behind the remembered disclosure. Preserve error/status,
URL and transport controls outside it. Show safe inline validation and concise
instructions. Do not change sign-in protocol or credential storage policy.

`Tests/App`, `Tests/Native`, `Tests/UI`: behavioral regressions, typed-state tests,
and safe actual-view rendering with injected fake credentials/engine. Never load
real credentials, use protocol logs or network during a screenshot/test.

## Execution and verification

1. Add failing tests for typed phases/history separation, raw invalid port text,
   stopping/duplicate Start, PIN rejection, discovery restarts and validation.
   Observe missing APIs/old behavior using focused native targets and direct
   app XCTest (`scripts/test-app-unit-direct.sh`); then implement minimal fixes.
2. Implement pure presentation and native views. Test all states, including healthy
   streaming with historical resets, and preserve protected-content recovery.
3. Render real SwiftUI views using only test fakes into ignored `build/` PNGs;
   inspect light/dark, normal/narrow main windows and setup sheets. Compile UI
   tests. Host automation limitation remains unless actual UI tests pass.
4. Run `python3 -m unittest discover -s Tests/Bootstrap -v`, native nontransport
   targets, app tests, focused transition/protected-content integration and
   ASan/TSAN as warranted by native changes. Time stages and parallelize independent
   work; retain Debug/Release cache isolation. Avoid repeated whole media matrices.
5. Package `scripts/package-dev-app.sh --config Release --output
   build/package-release-candidate`, validate bundle/signatures, docs and whitespace.
   Update user docs, explicit release checklist and independent-review handoff.
   Commit scoped changes on main. Publishing still requires separate authorization.

## Acceptance signals and limits

Current health is truthful; old output resets are secondary history. Every state
has a useful explanation and permitted action. Metrics never overflow or shift to
the far right at supported widths. Invalid inputs cannot start capture/pairing.
Search progress resets on every search. Passcode submission/dismissal cannot
accidentally stop recovered capture. All automatic tests avoid real secrets and
network. App/OBS latency, calibrated HDR colour, measured sync, long-run soak,
protected-app live confirmation, clean-machine launch and independent review
remain explicit gates unless new evidence is actually collected.

## Surprises & Discoveries

- Native currently conflates current errors, historical resets and source retry
  history in one 192-byte message; UI state cannot be reliable by string styling.
- Native callbacks dispatch state asynchronously, then Swift reads a newer status;
  old callback/new snapshot mismatches can cause incorrect transient UI.
- MainView uses a numeric TextField that may retain a previous valid integer while
  showing invalid text. Start must validate the raw text as well.
- PairingSheet's Search Again sets searching=true but only its initial task clears
  it. Invalid nonempty pairing codes currently leave Pair enabled.
- Discovery and pairing share one message, and a discovered selection can remain
  selected after the user edits its address.
- The passcode sheet's presentation setter stops capture on any false value,
  without checking whether the native PIN state has already been left.
- The single utility window now replaces WindowGroup to avoid multiple setup
  sheets mutating shared models; Dock reopen and Window → Show Capture Window
  restore it without stopping capture. Live Dock interaction remains manual.
- Unchanged periodic PIN status is not a submission acknowledgement. Submission
  progress now persists until a changed PIN response or a different native state.
- Controller review found a second transient failure path: connection exceptions
  still published FAILED before automatic retry. A new native regression fails
  in `build/release-polish-exception-red.log`; exceptions now publish RETRYING
  while true terminal failures retain FAILED. Focused native/sanitizer reruns are
  required after this small correction, not another whole transition matrix.
- Real settings screenshots revealed a duplicate visible Bitrate slider label;
  hide the redundant visual label while retaining its accessibility label.

## Decision Log

- Keep one native window and existing icon/feature set, as user selected.
- Prefer structured phases/history to parsing translated/free-form status strings.
- Keep setup locked until explicit Stop acknowledgement, including terminal error
  cleanup; do not enable credential mutations while native teardown may run.
- This is a release candidate, not authorization to publish or waive release gates.

## Outcomes & Retrospective

Initial red evidence: `build/release-polish-native-red.log` (missing typed fields)
and `build/release-polish-app-red.log` (missing validated input/lifecycle APIs).
`build/release-polish-views-first.log` proves missing PIN dismissal guard before
implementation. Model/view pass: `build/release-polish-ui-green.log`, 88 tests,
including actual off-screen rendering in 1 second using test fakes only. Native
nontransport targets pass in `build/release-polish-native-green.log`.
No UI automation, independent review or actual PS5 acceptance claim follows from
these initial renderings. Expanded verification and closeout evidence follow.

### Completed developer checkpoint

Final evidence: `build/release-polish-close-app.log` passes 88 app tests and
regenerates all 30 actual SwiftUI PNGs (synthetic data only). Final native run
passes nine targets including the new exception-retry regression. UI test target
compiles. Host `DevToolsSecurity -status` reports Developer Mode disabled; it was
not changed and interactive XCUITest is not claimed. Existing final focused
integration run passes five methods covering compatible/HDR/header transitions,
six long-gap/protected cases, output-reset reasons and explicit no-SDR fallback
(`build/release-polish-final-transitions.log`, 118.70 s). After the controller-review
exception fix, the affected native targets and app were rerun; media code did not
change again. Five focused ASan and five TSAN native targets pass; prior focused
sanitizer integration also passed all gap/protected and reset-reason cases.
Eleven policy/build tests pass in `build/release-polish-close-policy.log`.

Latest closeout stages: app 10.34 s, native 9.38 s, UI compilation 1.67 s, Release
packaging 13.13 s, strict signature 0.05 s. Sanitizer final native rechecks ran in
parallel (10.71/16.53 s); timings are in `build/release-polish*-timings.json`.

Candidate: `build/package-release-candidate/Free PS5 Capture Card.app`, version
0.1.0 build 2, original AppIcon, no test plugins, 28 arm64 Mach-O closure checks
and strict recursive signatures pass. Local archive:
`build/FreePS5CaptureCard-0.1.0-rc1-arm64.zip` (20,466,705 bytes); CRC/version/icon
checks pass; SHA256 `be35ba21a8876d36c3ff22ba83c55c04af77ec0a58e8f70cc56b82d7ebe6e066`.
Nothing was published, tagged, notarized or installed to Applications.

User-facing setup instructions and changelog are current; independent review
scope is in `docs/review-handoff.md`, remaining release gates in the checklist.
Next concrete step: hand over this RC for manual UI/device acceptance and obtain
independent review before authorizing public distribution. Preserve deferred
sync/HDR/latency/soak and clean-machine checks rather than converting them to
passes based on screenshots or synthetic tests.
