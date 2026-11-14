# Validation evidence

## Active OBS output-drop investigation

### Measured video clock mismatch and corrective candidate

The latest live screenshot reports SPS 59.94 fps versus 58.921/s video counter
progression, nominal video-clock drift -650 ms; audio is 100.001/s, 480 samples
per packet and 0 ms drift. Both tracks remain fresh (audio 10 ms/video 17 ms),
zero source retries/loss/hole counters, 9 OBS resets and audio-start +858 ms.
This identifies the fixed SPS-period video mapping as the divergent clock. It
does not measure end-to-end A/V synchronization or establish why the source's
delivered video counter cadence differs from SPS metadata.

The correction retains the audio sample/index PTS and initial phase, maps live
video ingress times onto that audio timeline using a bounded recent lower-envelope
audio-delay estimate, and freezes each video PTS when admitted. No queue/timeouts,
payloads, codecs/HDR selection, audio resampling or decoding are changed. This
supersedes the fixed video-counter mapping; SPS remains nominal codec metadata.
Ingress jitter/differential network delay still requires real sync assessment.

`build/clock-fix-red.log` records failed non-SPS cadence/burst timestamp assertions.
`build/clock-fix-pipeline-red.log` records failure to produce a readable Matroska
header in all three modes for a continuous synthetic 30-fps source whose SPS
declares 60 fps. Metadata is retagged in test-only FFmpeg; compressed media is not
captured from the user. The same native suite first passes all 10 targets in
`build/clock-fix-native-green.log`, including all-mode playback/reconnect regression.
Expanded tests cover two-hour virtual runs with ±500 ppm audio rate error, bounded
ingress jitter, video cadence changes, wraps, immutable/strictly ordered video
PTS, exact audio PTS and compressed-payload subsequence preservation.

Latest corrective candidate: `build/package-clock-fix/Free PS5 Capture Card.app`.
Final verification (`build/clock-fix-check-*.log`): 10 native targets, 72 app tests,
10 ASan targets, 7 selected TSAN targets and 7 policy tests pass; Release packaging
succeeds. `Tests/Support/check_bundle.py --app` validates 28 arm64 Mach-O files;
`codesign --verify --deep --strict` passes. The reproduced all-codec failure is
corrected in automated regression, but actual live OBS resolution, synchronization,
HDR/soak and independent review remain unverified release gates.

### Growing startup offset and counter-clock diagnostic follow-up

The user supplied two screenshots from an unchanged source session. The retained
startup audio-start offset grows from +1040 ms to +3351 ms; both tracks remain
fresh (10/18 ms, then 5/20 ms audio/video ages). Received counts advance from
2502/4279 to 10921/18549 video/audio; OBS resets increase from 8 to 71 with zero
source retries/loss/hole counters. This supports accumulated clock divergence,
but does not establish which counter/cadence mapping is wrong or actual A/V sync.

`build/clock_mapping_probe.cpp` uses the production Timeline to reproduce the
failure mechanism without live media. SPS 60000/1001 plus continuous 59-fps video
and 480-sample/10-ms audio yields -2823 ms video-duration-versus-receipt drift
after 180 seconds, while audio drift is zero. For 60-fps video it yields +179 ms.
The input rates are controlled hypotheses, not a claim about the user's PS5.
The pinned upstream playback code also estimates intervals, rather than treating
SPS as proof of transport cadence; no playback/decoder code is enabled here.

Follow-up status includes independently measured per-track counter spans,
elapsed receipt spans, signed nominal-duration drift and audio samples per packet.
Clock diagnostics are unaffected by a constant start phase, and retain unwrapped
positions across raw-counter wraps while excluding rejected duplicate/reordered
observations. They do not alter timestamps, clocks, reset limits or buffering.
Native missing-metric assertions failed in `build/clock-rate-red.log`; app bridge
assertions failed in `build/clock-rate-app-red.log`. The subsequent live screenshot
above identified the divergent video clock before implementing the correction.

Previous clock-diagnostic candidate: `build/package-clock-diagnostics/Free PS5 Capture Card.app`.
`build/clock-check-*.log` records 10 native targets, 72 app tests, 10 ASan targets,
7 selected TSAN targets and 7 policy tests passing, plus successful Release
packaging. `Tests/Support/check_bundle.py --app` passes for 28 arm64 Mach-O files;
`codesign --verify --deep --strict` passes. No live clock-rate evidence or timing
correction is claimed by these automated checks.

### Live retry and more specific track diagnostics

The user reproduced black OBS output with the TCP compatibility candidate.
The status screenshot reports 20 OBS resets, zero source retries/video losses/
audio holes, nonzero cumulative video/audio receive counters and the retained
MissingTrack message. This confirms a local queue reset, not its specific trigger:
the old text conflates missing configuration, startup timestamp alignment, and
steady-state audio/video delivery gaps. Cumulative receive counts do not prove
uninterrupted delivery or accepted timestamp progression.

The follow-up captures the missing-track stage, per-track queue receipt ages and
the startup audio PTS minus starting video PTS before reset clears queue state.
Status text includes these bounded numeric diagnostics, not payloads or protocol
strings. Ages are sampled at the reset, not a continuously updating measurement;
before a track's first admitted packet, age is relative to OBS attachment.
The startup offset is the last admitted audio PTS minus the starting keyframe
PTS, not measured A/V sync. Thresholds and queue/reset policy remain unchanged.
Expected missing details failed in `build/track-detail-red.log`; expected detailed
messages failed in `build/track-message-red.log`. Specific live cause and remedy
remain pending a retry with the more detailed diagnostic candidate.

Previous track-diagnostic candidate: `build/package-track-diagnostics/Free PS5 Capture Card.app`.
Verification in `build/track-check-*.log`: 10 native targets, 71 app tests,
10 ASan targets, 7 selected TSAN targets and 7 policy tests pass; Release packaging
succeeds. `Tests/Support/check_bundle.py --app` validates 28 arm64 Mach-O files,
and `codesign --verify --deep --strict` passes for this bundle. The initial run
was user-cancelled during TSAN, not a spontaneous test failure; unfinished stages
were subsequently rerun successfully. Live resolution remains unverified.

User reports repeated OBS disconnects while the app returns to Waiting for OBS.
The symptom persists after reducing only bitrate from 60 to 15 Mbps. Relevant
sanitized OBS media-player lines confirm successful localhost connections and
later drops; earlier invalid 92950 inputs are not the whole explanation. Existing
client code did not retain a local output-reset cause, so no particular source,
queue, socket or OBS condition is yet established as this user's root cause.

Instrumentation adds a static last-reset message and per-source OBS reset counter
without altering timeouts, queue limits, media timestamps or codecs. Synthetic
tests distinguish a deliberate peer close from a 350 ms source-delivery pause,
on temporary test ports only. Expected missing diagnostics failed first in
`build/output-diagnostics-red.log`, `build/output-tcp-red.log` and the corrected
`build/output-app-red.log`. Real reproduction with a reported reset message is
still needed; this is not a claimed fix for the user's disconnects.

An additional socket experiment did reproduce an app defect: a receiver that
half-closed its unused sending direction was disconnected before getting a
Matroska header. `test_pipeline_accepts_receive_only_tcp_peer` failed for that
exact reason in `build/receive-only-red.log`. The liveness check now accepts TCP
read EOF; existing bounded writes detect actual receiver closure/failure. This
fixes that reproduced protocol case, but OBS's use of half-close in the reported
live session has not been established. Real disconnect resolution is pending.

Earlier TCP compatibility candidate (2026-11-14):
`build/package-obs-diagnostics/Free PS5 Capture Card.app`.
`scripts/test.sh --native` passes all 10 targets;
`scripts/test-app-unit-direct.sh` passes 71 tests;
ASan CTest passes 10 targets and selected TSAN CTest passes 7 targets;
`python3 -m unittest discover -s Tests/Bootstrap -v` passes 7 policy tests.
Results are retained in `build/obs-check-*.log`. Release packaging succeeds;
`python3 Tests/Support/check_bundle.py --app 'build/package-obs-diagnostics/Free PS5 Capture Card.app'`
passes for 28 arm64 Mach-O files and
`codesign --verify --deep --strict` passes for that bundle. These checks do not
establish GUI launch, live PS5/OBS reliability or independent review acceptance.

## Current credential-file follow-up — 2026-11-14

Credential-storage checkpoint app: `build/package-file-store/Free PS5 Capture Card.app`
(superseded by the OBS diagnostic candidate above).
This build supersedes the earlier Keychain-backed artifacts below. It starts
fresh, without reading/migrating/deleting existing Keychain records, as explicitly
approved by the user. Account identity and PS5 keys now use an unencrypted file
in a 0700 directory with 0600 file permissions. This is weaker than Keychain and
does not prevent same-user processes from reading credentials.

- `build/file-store-red.log`: missing persistence failed before implementation.
- `build/file-store-green.log`: initial backend passed 65 app tests.
- `build/file-store-app.log`: final 70/70 app tests pass, including real isolated
  filesystem checks, stale writer/deletion guards, account replacement, mode/link/
  FIFO/size/schema validation, lock contention, corrupt reset, and proof that
  successful mocked login persists identity without access/refresh tokens/codes.
- `build/file-store-native.log`: 10/10 native targets pass; native media unchanged.
- `build/file-store-policy.log`: 7/7 entry-point/policy tests pass, including no
  application credential-Keychain APIs/imports and no decoder/device calls.
- `build/file-store-package.log` plus fresh bundle checker: Release app,
  28-file recursive runtime closure, minimum OS, architecture and signatures pass.
  Release imports no direct `SecItem*` functions, contains no old credential service
  ID, and excludes the Debug UI-test launch flag. System TLS/framework dependencies
  are not evidence of app Keychain credential access.
- All stores in tests are injected fakes or unique temporary directories. No real
  user credential file, Keychain, PSN account or console was accessed.

Fresh real sign-in/pairing/relaunch persistence, GUI automation, clean-machine
launch, independent review and all existing real-media acceptance gates remain
open. Earlier user-confirmed Keychain recovery does not validate the new backend.

## Address-update follow-up

The latest developer bundle is
`build/package-address-update/Free PS5 Capture Card.app`. It adds stopped-only
**Change IP…**, preserving existing account/console identity and all keys without
discovery, registration or a new login. Save does not claim network connectivity.

- Missing update behaviour failed first in `build/change-ip-red.log`.
- Deletion-during-save exposed record resurrection through generic upsert;
  the narrow update-only path fixes it (`build/change-ip-no-upsert-red.log`).
- `build/change-ip-app.log`: 56/56 app unit tests pass, including preservation,
  relaunch, IPv4/IPv6 validation, denied writes, stale/forgotten/replaced records,
  busy pairing and active-capture guards.
- `build/change-ip-native.log`: 10/10 native targets pass (native code unchanged).
- `build/change-ip-package.log` and fresh bundle checker: Release build,
  28-file recursive runtime closure, minimum OS, architecture and signatures pass.
- GUI automation and the existing real-media/clean-machine/independent-review
  gates below remain open; no actual user Keychain or console was used by tests.

## Current continuation snapshot — 2026-11-13

**Final handoff artifact:** `build/package-handoff/Free PS5 Capture Card.app`.
The latest checks supersede older candidate paths/logs below:

- `build/verified-native.log`: 10/10 native targets pass.
- `build/verified-app.log`: direct Debug XCTest, 47/47 pass.
- `build/verified-asan.log`: ASan/UBSan, 10/10 targets pass.
- `build/verified-tsan.log`: TSAN, 7/7 selected targets pass.
- `build/verified-package.log` and a fresh `check_bundle.py` invocation:
  recursive closure/minimum-OS/architecture/signature checks pass, 28 Mach-O files.
- Final code initializes process-wide FEC/cache state exactly once before live
  setup, reports queue age and both loss/hole counts, wipes copied credentials on
  Stop, and preserves explicit server registration rejection as a nonretryable
  identity failure. Concurrent initialization and typed rejection tests pass.
- Locked patch SHA-256 and fresh sequential application reproduce every tested
  changed core file byte-for-byte in `build/patch-verify-handoff`.

These are development checks, not real-device or independent review approval.

Historical milestones below are a chronological log, not the current feature
list. The live session and native UI are now implemented; they are not yet
accepted on real PS5 media.

- Real core ownership (init/sink/start/stop/join/fini), port-before-console,
  transient login PINs, retries and both-track receiver/TCP output are wired.
- `build/live-session-regression.log`: full native/app pass after session wiring.
- `build/recovery-regression.log`: 10/10 native CTest targets pass, including
  upstream core, receiver, clocks/queue, lifecycle, diagnostics and transport.
  Recovery tests cover delayed audio config, missing audio, OBS reconnect,
  cancellation in backoff, 100 raw-loopback reconnects, rejected second client,
  cancellable accept/send and a nonreading client's bounded write. The 100-cycle
  socket stress is not 100 decoded PS5 or full receiver-pipeline sessions.
- `build/sanitize-live-tests.log`: fresh ASan+UBSan build, all 10 CTest targets
  pass, including the receiver pipeline. No sanitizer failure was suppressed.
- `build/ui-regression.log`: 45 app unit tests pass; **XCUITest did not run**:
  runner initialization timed out enabling macOS automation. Do not count this
  as a passing UI smoke or a product-test assertion failure. No system automation
  permission/service was changed on the user's machine.
- Release developer packaging passes recursive closure checks: **26 copied
  non-system libraries, 28 Mach-O files**, arm64, no external runtime dependencies
  or rpaths, compatible declared minimum OS and ad-hoc signatures. See
  `build/package-release.log`. Current project targets explicitly reflect the
  actual macOS 26 dependency floor; older macOS remains unsupported.
- Locked patch digests and sequential application pass in a fresh isolated
  checkout under `build/patch-verify-final`; the reference checkout is untouched.
- Source-policy/entry-point suite: 6/6 pass. Codec-opening, decode/encode/device
  calls are forbidden in app/native sources. Linked FFmpeg codec symbols alone
  are neither proof of decoding nor independent call-boundary review.
- Final native run: 10/10 pass in `build/final-native-app-rerun.log`. Subsequent
  Xcode unit-host runs stalled before tests while XCTest awaited its IDE session;
  sampled stacks show `_prepareTestConfigurationAndIDESession`, with the capture
  worker idle. The explicit direct Debug XCTest invocation passes **47/47** in
  `build/direct-unit.log`, including new exact mode/bitrate/port/persistence cases.
  This is not app-launch/UI acceptance. A named unit-only alternative script
  documents the distinct path instead of hiding the normal harness failure.
- Final refreshed native sanitizer matrix passes in `build/final-asan-tests.log`
  (10/10) and `build/final-tsan-tests.log` (7/7). The explicit direct unit script
  builds and passes 47/47 in `build/direct-unit-script.log`.
- Final Release package is `build/package-final/Free PS5 Capture Card.app`;
  `check_bundle.py` passes again after adding full embedded Chiaki/gf-complete/
  Jerasure/nanopb notices. Release contains no Debug UI-test launch flag, and
  `nm -u` on CaptureEngine finds no direct codec-opening/decoding/encoding imports.
  The CI workflow is configured but has not been run remotely.
- One parallel cold native/TSAN run exceeded a pre-existing 3-second test-tool
  startup timeout for invalid port input. Direct rechecks exit correctly in
  0.03–0.08 seconds; sequential native and TSAN reruns pass. No production timeout
  was raised to hide the failure.

### Still required before release/acceptance

- Actual PS5 → OBS smoke for H264, H265 default and H265 HDR, with picture and
  source-mixer activity for audio arrival. Do not repeat already-confirmed basic
  pairing/relaunch or synthetic-OBS feasibility merely as a development gate.
- Real encoded cadence/reorder/Opus behaviour; timing measurements near minute 1
  and minute 30 for each mode: residual A/V mismatch ≤40 ms, drift ≤20 ms.
  Measure any fixed correction rather than assuming arrival-offset calibration
  establishes phase. Current UI has no A/V correction control.
- Genuine HDR source/output bit depth, range, PQ/BT.2020 and signalled static
  metadata; calibrated/visual output checks. No hidden fallback or app tone map.
- Real high-bitrate/720p/1080p/30/60 behaviour, consumer/network recovery latency,
  RSS/CPU trends and OBS drops. Targets remain ≤10 s consumer recovery and ≤20 s
  source recovery after network return, not guarantees under arbitrary delays.
- PS5 login prompts, controller/TV/account behaviour, saved-key operation without
  fresh OAuth/Internet, macOS sleep/wake and real permission-denial scenarios.
- Manual accessible/light/dark/keyboard UI inspection, window reopen/quit and
  working XCUITest automation environment.
- Packaged clean-account/machine launch without developer paths; real onboarding
  and permissions. Recursive static closure checks do not establish this.
- Independent code/security/license review, actual FFmpeg configuration and all
  transitive notice/source obligations. Nothing is published, installed into
  Applications, notarized or enabled at login by this work.

## Baseline setup

- Architecture: arm64.
- Xcode: 27.2 (27B52); Swift: 6.4.2.
- Homebrew: CMake 4.5.0, Ninja 1.13.2, XcodeGen 2.46.0,
  FFmpeg 9.0.2_1, pkgconf 3.0.10, protobuf@29 29.6_2,
  OpenSSL 3.5.9, json-c 0.19, miniupnpc 2.3.3, libevent 2.1.13_1,
  Python 3.13.18.
- Isolated Python protobuf: 5.29.6 (`build/python`, not global pip).
- FFmpeg advertises libx264, libx265, and libopus encoders, needed for synthetic
  fixture generation. This does **not** prove 10-bit HDR fixture generation or
  live OBS ingestion. FFmpeg's Homebrew package installed SDL transitively;
  the capture core disables SDL/controller/GUI use and must not initialize it.
- Homebrew warns protobuf@29 is deprecated and scheduled for disablement on
  2027-03-31. Generator upgrades must be tested explicitly before replacing it.

## Checks performed

1. `python3 -m unittest discover -s Tests/Bootstrap -v`: four failures before
   the build entry point existed; four passes after implementation.
2. `bash -n scripts/bootstrap.sh scripts/build-native.sh`: pass.
3. Dependency patch SHA-256 matches `Dependencies.lock.json`.
4. `git apply --check` against the pinned reference checkout: pass (read-only;
   patch not applied to that checkout).
5. `scripts/bootstrap.sh`: pass; creates only the ignored isolated Python
   environment and installs its pinned protobuf.
6. `scripts/build-native.sh --config Debug --tests`: pass, 263 build steps.
   The first attempt was cancelled by the user; the explicitly requested restart
   completed. GUI, CLI, FFmpeg decoder, and Opus playback remain OFF in the
   generated CMake cache.
7. `ctest --test-dir build/native --output-on-failure`: pass, 1/1 CTest target.
8. `build/native/_deps/chiaki-build/test/chiaki-unit`: **118/118 tests pass**,
   zero skipped. These are core regression tests, not live PS5/OBS tests.

## Build warnings / compatibility limits

- Several linked Homebrew libraries have a macOS 26 minimum, despite the requested
  15.0 deployment target. This is a current-Mac development build, **not verified
  macOS-15 support**. Older-OS distribution requires compatible dependency builds
  or an explicit user-approved change of minimum OS before release.
- Existing upstream curl uninitialized-pointer, OpenSSL deprecated API, Gestalt,
  and GNU array-extension warnings remain visible; they did not fail this build.
  They have not been fixed or suppressed in the reference checkout.

## Pending checks

- Remaining app functionality, visual/UI automation and distributable dependency checks.
- Synthetic video/audio transport, all three codec modes, and OBS ingestion.
- Real PSN login, console pairing, A/V synchronization, HDR, and reconnect gates.
- macOS-15 compatibility: the requested target is not proof that installed
  Homebrew binaries run on that OS; check bundled deployment metadata later.

## Native interface and app shell milestone

- The baseline was pushed to `origin/feat/native-app-shell`; no push to main.
- Native contract red run: 0/3 cases passed with deliberately unimplemented
  stubs. Then real worker/lifecycle/validation implementation: 3/3 pass.
- App contract red run: five tests failed for missing model/adapter behavior;
  a second red run exposed two missing shutdown guarantees. Final result:
  7/7 app tests pass, including real framework callbacks on the main actor,
  no automatic capture, unavailable capture reporting, and joined shutdown.
- Full `scripts/test.sh --native --app`: pass, CTest 2/2 targets (118 upstream
  core cases + 3 interface cases) and XCTest 7/7 cases.
- `scripts/build.sh --config Release`: pass. Debug build also passes as part of
  the full test command. XCTest launches the app as its test host.
- `python3 -m unittest discover -s Tests/Bootstrap -v`: 4/4 pass.
- `codesign --verify --deep --strict` on both final Debug and Release apps: pass.
  Ad-hoc development signing is not notarized distribution approval.
- `otool -L` on the engine shows no Qt/SDL/rendering library dependencies; local
  Homebrew runtime dependencies remain and are not yet bundled.
- macOS default Bash exposed an empty-array expansion issue in the unfiltered
  app test command. Reproduced failure, fixed to use a nonempty argument array,
  and verified the full unfiltered command passes.
- Xcode reports an outdated **iOS** simulator service and missing optional
  AppIntents metadata; the selected destination is native macOS and tests pass.
  No simulator upgrade/system service change was performed.

The app is a runnable development shell, NOT a functional capture app. No PSN,
pairing, or live-PS5 media capability has been verified. Visual
inspection and independent code review are still pending.

## Synthetic transport implementation (OBS compatibility confirmed by user)

- Working branch is now `main`, as explicitly requested by the user. No extra
  milestone branch or PR is required to continue development.
- Initial red integration run failed for the missing fixture generator/sender.
  TCP tests subsequently failed for unsupported server options. Additional red
  tests caught a queued second consumer and overwriting of existing debug output.
- HDR validation caught missing transfer/primaries in the encoded fixture VUI.
  Bitstream tracing showed unspecified values; explicit x265 colorprim/transfer
  parameters corrected the stream. Frame-level probing verifies PQ/BT.2020,
  mastering metadata, MaxCLL, and actual decoded patch codes. Stream-only probing
  can omit fields resolved after the first decoded frame.
- `python3 Tests/Support/integration.py --build-dir build/native --case all`:
  six test methods pass, exercising H264 SDR, H265 SDR, and H265 HDR subcases.
  Compressed packet hashes match on both remuxed tracks for all modes; validation
  decoding is test-only. Production mux/server/replay code opens no codec context.
- TCP receives both tracks in a separate FFmpeg process. Tests cover ten fresh
  header reconnects per mode, second-client rejection, port errors, read-only
  fixture preservation, and cancellation within two seconds. An eleven-second
  H265 capture crosses the ten-second fixture loop without timestamp rewind.
- Full `scripts/test.sh --native --app`: CTest 3/3 targets pass (118 core, 3
  interface cases, 6 integration test methods), and XCTest 7/7 pass. Bootstrap
  tests remain 4/4 green. Fixtures are small 320x180/30 fps synthetic samples.
- The user tested the live feeds at ports 9295 (H265 SDR), 9296 (H264 SDR), and
  9297 (H265 HDR) and reported that all work. Combined with the observed Media
  source meter, this confirms basic A/V ingestion in the installed OBS setup for
  all three required modes. The transport compatibility checkpoint is accepted
  for progressing to account setup; this is not a calibrated HDR-output or
  precise lip-sync measurement.
- Pending: measured A/V sync and correct final HDR output/color configuration,
  high-bitrate backpressure/soak, live Annex-B delivery, and PS5 clock behavior.
  These are not inferred from FFmpeg ingestion or packet hash preservation.

### User OBS screen-recording observation

The user supplied a 5.84-second macOS screen recording. Extracted frames show
the test pattern moving in the OBS canvas, with its displayed time advancing
approximately one second per second. Mixer activity is visible. This is evidence
that the displayed test video is reaching OBS, not yet end-to-end A/V acceptance.

`ffprobe` reports only one H264 video stream in the submitted screen recording;
there is **no audio track**. Consequently the clip cannot verify the test tone,
the audible result or precise audio/video synchronization. On closer inspection,
the **Media source's own mixer meter is active**, not only microphone/desktop
meters. Together with moving video this supports the basic OBS A/V arrival smoke
check; the lack of an audio track in the screen recording does not invalidate
that visual evidence. A direct OBS recording would be needed for waveform/listening
analysis or precise synchronization measurements, not merely to establish meter
activity. The user subsequently confirmed all three codec feeds work in OBS.
Measured HDR output, exact synchronization, and manual reconnect checks remain
separate validation items. The private
screen recording was not copied into Git.

## Native account setup implementation (manual Gate B pending)

- Added native browser-based sign-in, strict redirect/code/state validation,
  bounded ephemeral HTTP transport with redirects/cookies/cache disabled,
  precision-safe 64-bit account identity conversion, and versioned Keychain
  storage. OAuth tokens and redirect URLs are not persisted.
- Initial red run: the 11 new auth/storage tests executed and failed against
  stubs. Four new account-model tests then failed against an empty model.
  Subsequent implementation and broader edge-case checks pass.
- Full `scripts/test.sh --native --app`: CTest 3/3 targets pass (118 core, 3
  interface cases and 6 transport test methods), XCTest 25/25 pass. Bootstrap
  4/4 pass. Tests cover malformed/duplicate/wrong-state redirects, expiration,
  one-use codes, form escaping, cancellation/stale responses, numeric/string
  uint64 precision, denied/corrupt storage, and restore/forget behavior.
- Unit tests use fake HTTP and Keychain boundaries; no real Sony credentials or
  account data were used, and actual login/Keychain entitlement behavior is NOT
  established. Live console pairing remains unimplemented.
- Final Debug/Release builds and ad-hoc signature verification pass, including
  the scrollable account window. Full handoff test run again passes CTest 3/3
  targets and XCTest 25/25 cases.
- See `docs/account-setup.md` for the real sign-in/Keychain/relaunch gate. If Sony
  omits/rejects state or changes the API, stop and investigate rather than
  disabling validation. Synthetic replay feeds remain stopped as requested.

## Local discovery and pairing implementation — Gate C pending

- Added pinned-core registration/discovery adapters with copied values, owned
  workers and joined teardown. Raw registration logging is disabled. Discovery
  only recognizes explicit permission failures without forwarding raw log text;
  an empty list is not reported as permission denial.
- Added the native pairing sheet, numeric-IP fallback, leading-zero-preserving
  eight-digit code validation, account/MAC/target checks, and one atomic Keychain
  record for account plus optional console. Legacy account records migrate on
  write. Failed/cancelled replacement, stale callbacks and denied saves preserve
  the existing pairing. Forgetting account deletes console keys too.
- Red evidence: `scripts/test.sh --native --filter pairing` initially failed all
  four stubbed cases. Initial Swift pairing/model/store tests failed against
  stubs, then native bridge conversion tests failed against empty conversions.
  Additional red runs reproduced discovery permission suppression, missing key
  payload acceptance, and shutdown returning while a native join was outstanding.
- Final `scripts/test.sh --native --app`: 4/4 CTest targets and 41/41 XCTest cases
  pass. Pairing adds 5 native test cases with multiple lifecycle/error modes and
  16 app cases. Bootstrap 4/4 pass. Final Release build passes.
- Debug/Release ad-hoc signature checks pass. A separate `build/native-asan`
  configuration instruments C/C++ with `-fsanitize=address,undefined
  -fno-omit-frame-pointer`, reuses the pinned fetched source via
  `FETCHCONTENT_SOURCE_DIR_CHIAKI`, and builds `capture-pairing-tests`.
  `ctest --test-dir build/native-asan -R '^capture-pairing$' --repeat
  until-fail:10 --output-on-failure` passes all 10 runs. This covers injected
  setup workers/copies, not real network registration, Swift, or all media code;
  TSAN has not been run. No sanitizer finding was reported.
- User-guide audit before/after reports no broken links in its managed scope,
  but this app has no cookbook-managed guide tree (`docs/skills/README.md` is
  absent). Pairing instructions and their README/account-guide links were checked
  directly rather than adding an unrelated cookbook documentation tree.
- No real console/network discovery, pairing PIN, or real Keychain data was used
  by automated tests. Manual PS5 registration, quit/relaunch persistence, actual
  permission behavior, and invalid/expired-code UX remain Gate C. See
  `docs/pairing.md`. Live media and TV/controller/account behavior are not yet
  tested; Start Capture remains disabled. Independent review is requested in
  `docs/reviews/pairing.md`, not claimed completed.

## Real pairing user report

After the pairing build, the user reported **paired securely**. After the app
was reopened they confirmed **yep still paired**. This accepts real registration
and user-reported Keychain recovery for progressing beyond Task 4, not complete
discovery/manual-IP/failure-path coverage or proof of live audio/video.

## Encoded-media delivery — Task 5

- Added `0002-encoded-media-sink.patch` and its SHA256 to the dependency lock.
  Alternate video/audio configuration and packet callbacks include track
  generations, source frame index, first receiver-entry monotonic time, samples,
  explicit audio holes, and video/audio recovery metadata. Audio preserves entry
  time through jitter; config/frame callbacks execute outside its mutex.
- Alternate-sink conflicts are rejected at installation and before start. Legacy
  callbacks remain available and are covered through the same real receivers.
  Video callbacks still govern reference acceptance. An owning C++ adapter applies
  64 KiB-header/4 MiB-packet caps before copying; default discard accepts frames.
- IDR transport access is guarded across stop/close. The lifecycle owner can
  detach under a request gate, then join/finalize outside receive callbacks.
  Threaded tests issue requests during stop/detach and after session finalization.
- Red evidence: `scripts/test.sh --native --filter media-sink` first failed five
  new delivery/conflict/acceptance tests while the legacy regression passed.
  Later runs failed the inactive-transport IDR expectation and all three empty
  adapter/control stubs. The actual wrap-loss regression then produced -65534
  instead of 2 after a failed FEC frame across the 16-bit wrap; sequence arithmetic
  fixes it. The initial FEC test fixture's padding/parity layout was corrected
  separately, not treated as a production receiver bug. An added HEVC rewrite
  regression initially used a single-reference fixture that cannot encode a
  fallback reference; inspection of the RBSP and upstream rewrite policy led to
  using upstream's nine-reference fixture instead. Both HEVC mode values now
  verify the existing reference rewrite and its metadata flag without changing
  the library's recovery policy.
- Final `scripts/test.sh --native --app` passes 5/5 CTest targets and 41/41
  XCTest cases; `capture-media-sink-tests` reports 14/14 cases. Bootstrap 4/4 pass.
  Debug/Release builds and ad-hoc signature verification pass. Existing unrelated
  dependency warnings/macOS simulator diagnostics remain; no new native adapter
  compiler failure is hidden.
- Independent fetch/configure/build in `build/native-media-clean` passes all 5
  native CTest targets. Checked pinned revision, both locked patch hashes,
  already-applied reverse checks, and byte-for-byte equivalence of all 11 patched
  files against the development dependency. Removing only the generated media
  patch from that disposable tree and reconfiguring reapplied the locked series.
  The reference checkout was never modified.
- ASan/UBSan `capture-media-sink` in `build/native-asan` passes 10 repeated runs
  with no reported finding. This is injected-receiver/lifetime coverage, not live
  networking; TSAN and end-to-end session races remain future verification.
- Source search under `Native/src` finds no media decoder/encoder-open or
  send/receive/decode/encode calls. It is not a full future packaged-binary audit.
  Actual HDR/cadence/phase, shared A/V timeline, combined bounded queues, and
  live Matroska session delivery are NOT yet established. Capture remains
  disabled; no test streamed the user's PS5. See the pending review request in
  `docs/reviews/encoded-media.md`.

## Task 6 partial checkpoint — shared clock and owned storage

- Added 5 Timeline, 12 PacketQueue and 5 codec-unit cases. Initial red runs used
  `scripts/test.sh --native --filter timeline`, `--filter queue`, and
  `--filter codec-units` against empty implementations. Additional red tests
  exposed premature reset on initial headers, capacity-driven pre-roll reset,
  missing configuration epoch, a fresh source-counter reset ignored forever as
  reordering, and stale pre-roll incorrectly producing readiness. All are fixed.
- Timeline uses exact rational/sample progression through repeated source-counter
  wraps, a 250 ms frozen least-delay mapping and one shared media origin. Tests
  retain audio holes, positive/negative fixed offset and cross-track phase on
  reconnect; no accumulated per-frame rounding is used. This is constructed
  source-clock evidence, not measured real-console synchronization.
- Owned queue storage is capped before copying, including fixed metadata,
  configuration buffers and leases outside the queue. Tests fill all logical
  entries with leased buffers, then pass an invalid pointer to prove denial
  occurs before copying. Buffers remain valid after queue destruction and are
  charged until their final reference releases. Logical storage accounting and
  conservative metadata allowances are not a claim of a strict process-RSS cap.
- `python3 Tests/Support/integration.py --build-dir build/native --case
  synthetic-clock` passes its C++ two-hour clock/queue suites. The `all` case now
  includes this check as well as the existing six synthetic transport methods.
  Existing remux tests remain unchanged evidence for the old fixture sender;
  they do not prove the new live queue/mux connection works.
- Full app/native regression run passes 8/8 CTest targets and 41/41 XCTest cases;
  bootstrap 4/4 passes. Final Debug/Release builds and both ad-hoc signature
  checks pass.
- Separate `build/native-asan` uses ASan/UBSan; timeline/queue/codec targets pass
  10 repeats each. Separate `build/native-tsan` uses `-fsanitize=thread
  -fno-omit-frame-pointer`; `TSAN_OPTIONS=halt_on_error=1` with
  `capture-queue-tests --no-fork` and `capture-timeline-tests --no-fork` passes
  without reported races. These are synthetic/injected tests, not live network
  receiver or end-to-end shutdown coverage.
- NAL-header utilities classify true H264/HEVC random access and leading pictures
  without decoding; they do not parse actual SPS/VUI cadence, reorder capacity
  or HDR. OpusHead/TOC-duration helpers establish the stereo/48 kHz, pre-skip-0
  candidate only; live packets/startup alignment remain untested.
- **Task 6 is not complete:** codec metadata validation, borrowed-callback queue
  integration, measured mux/AVIO/cluster memory bounds and live Annex-B/Opus
  remux equivalence remain. The output owner must react to queue reset epochs and
  recreate both track states; the queue itself does not close sockets. Capture
  remains disabled, and no PS5 session was started. Review scope is in
  `docs/reviews/timing-storage.md`.
