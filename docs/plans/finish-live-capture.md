# Finish standalone PS5 capture

## Active OBS disconnect investigation

### Counter-clock investigation after user continuation

The user approved continuing the clock-mapping investigation, not replacing the
compressed transport with Syphon. Confirmed assumption in `Pipeline.cpp`: video
PTS uses unwrapped transport frame indices multiplied by parsed SPS frame period;
audio PTS uses unwrapped audio indices multiplied by validated Opus sample duration.
Neither output clock is checked against observed ingress cadence after freezing.
The pinned upstream `lib/src/ffmpegdecoder.c` separately estimates video intervals;
that playback implementation is evidence that nominal FPS is not sufficient, not
code to enable or copy (our app still must not decode).

Controlled experiment `build/clock_mapping_probe.cpp`, compiled with the real
`Native/src/Timeline.cpp`: with SPS 60000/1001 and continuous 59-fps video plus
480-sample/10-ms audio, the video clock falls 2823 ms behind receipts over 180 s,
while audio clock drift is zero. With continuous 60-fps video, it instead leads
by 179 ms. This reproduces a mechanism consistent with the growing live offset,
but does not establish the user's actual per-track cadence or defective clock.

Next bounded diagnostic implementation: expose each clock's unwrapped position,
elapsed first-to-last receipt time and media-duration-minus-receipt-duration drift,
plus negotiated audio samples per packet. Show these nonsecret numbers in the
existing status readout. Keep timestamp output, calibration, buffering and reset
policy unchanged until actual cadence evidence identifies the incorrect mapping.
No media or protocol capture, decoder use, or credential access.

- [x] Controlled counter-rate mismatch reproduction with the actual Timeline.
- [x] Red native tests for drift, phase independence, wraps and duplicate/reorder
  exclusion (`build/clock-rate-red.log`).
- [x] Red app status bridge test (`build/clock-rate-app-red.log`), then minimal
  native/status/UI implementation. Clock span/drift computation is read-only;
  it does not change existing output PTS or frozen calibration. Full verification
  and `build/package-clock-diagnostics` packaging run in the background.
- [x] Automated verification: 10 native targets, 72 app tests, 10 ASan targets,
  7 selected TSAN targets and 7 policy tests pass (`build/clock-check-*.log`).
  Release candidate `build/package-clock-diagnostics/Free PS5 Capture Card.app`
  passes bundle validation for 28 arm64 Mach-O files and strict recursive
  signature verification. Live clock diagnosis and capture resolution remain open.
- [ ] Live rate evidence identifies clock divergence before a timing correction.

### Live retry after TCP compatibility fix

The user reproduced black output on the diagnostic candidate. Screenshot:
20 OBS resets, zero source retries/lost-video/missing-audio counters, retained
`a required media track did not arrive in time`, with both receive counters
nonzero. The TCP compatibility correction therefore did not resolve this live
case. This establishes the queue's MissingTrack path, but not which track or
whether the failure occurred during timestamp alignment rather than delivery.
Passive socket inspection found the app and OBS connected on localhost; no extra
consumer was attached. Do not infer continuous delivery from cumulative counts.

Follow-up design/plan (within the approved diagnostic scope): preserve a bounded
reset snapshot before queue state is cleared, distinguishing missing configuration,
startup alignment and audio/video/both delivery gaps. Include per-track receipt
ages and startup audio-minus-video PTS offset where available. Keep existing
thresholds, queues and output policy unchanged. Prefer this to increasing timeout
limits without evidence or recording live payloads.

- [x] Red tests for startup-vs-delivery reset details and snapshot retention
  (`build/track-detail-red.log`, `build/track-message-red.log`).
- [x] Thread the reset snapshot through existing sanitized status text. New queue
  cases and message tests pass; first full regression found only two test-harness
  expectations to update (queue case count 12→14 and the old generic message).
- [x] Verify native/app/sanitizer/package checks: 10 native targets, 71 app tests,
  10 ASan targets, 7 selected TSAN targets and 7 policy tests pass. The user
  cancelled the first complete run during TSAN; only unfinished stages were
  resumed after continuation. Logs: `build/track-check-*.log`. Candidate:
  `build/package-track-diagnostics/Free PS5 Capture Card.app`; 28 arm64 Mach-O
  bundle validation and strict recursive signature checks pass.
- [ ] Establish the specific live failure from the detailed reset before fixing it.

Detailed live screenshot: startup alignment failed, audio age 10 ms, video age
18 ms, audio-start +1040 ms; 2502 received video packets, 4279 audio packets,
8 OBS resets, zero source retries/loss/hole counters. Both tracks were fresh in
the queue at the failed alignment. The +1040 ms value compares latest audio PTS
to the starting keyframe, not same-instant A/V sync. Candidate causes are a bad
initial phase estimate or accumulated divergence of frame-index/sample clocks;
SPS cadence alone does not establish transport frame-index timing. Next cheap
experiment: compare another reset snapshot after ~20 seconds without restarting
or changing settings. Growing audio-start suggests divergence; a stable value
suggests a persistent phase/startup-boundary problem. Neither conclusion alone
proves which source clock is wrong. No timing-policy change is justified yet.

The next unchanged-session screenshot reports audio-start +3351 ms (previously
+1040 ms), fresh audio/video ages 5/20 ms, 10921 video / 18549 audio packets,
71 OBS resets and zero source retries/loss/hole counters. The growing startup
offset with fresh tracks supports accumulated clock divergence, not a fixed
startup phase alone. Since these are reset-time snapshots, not simultaneous
source-clock measurements, the exact faulty cadence/index mapping remains to be
established. User asked whether Syphon would be easier; no transport redesign
has been authorized. Syphon carries decoded GPU video rather than this compressed
two-track stream and would require app-side decoding plus a separate audio path,
changing the current no-decoding/single-source constraints.

User reports OBS repeatedly disconnects while the capture app returns to
Waiting for OBS, including after reducing requested bitrate from 60 to 15 Mbps.
Sanitized local OBS log inspection confirms successful connections followed by
drops on the correct localhost port. A few earlier opens used an invalid 92950
port, but that does not explain the successful-connection drops. No credential
file, live media or additional client connection was accessed during diagnosis.

The previous build could not distinguish peer close, stalled write, stale queue,
missing-track delivery or startup-keyframe timeout: `Pipeline.cpp` discarded those
reasons when closing output. This follow-up adds instrumentation, not guessed timing
changes: retain a static sanitized last-output-reset message plus a per-source
OBS reset counter in the existing status, and surface it through the existing
Swift status readout. No protocol logging, media capture, extra devices or timing/
memory-budget changes. Test with local synthetic peer-close and source-pause
faults on temporary nondefault ports, never attach to the user's active endpoint.

A further controlled socket experiment reproduced a concrete transport defect:
a read-only receiver calling `shutdown(SHUT_WR)` received zero Matroska bytes
because `clientAlive()` interpreted receive EOF as the whole connection closing.
TCP send-half closure still allows the peer to receive. Regression
`test_pipeline_accepts_receive_only_tcp_peer` failed in `build/receive-only-red.log`.
The minimal correction accepts read EOF and relies on existing bounded writes
to detect a fully closed/nonreading receiver. No timing or storage limit changes.
Whether this behavior caused the user's OBS drops remains unconfirmed.

- [x] Red tests for reset diagnostics and synthetic peer/pause faults, plus a
  receive-only TCP half-close regression. A missing test import was corrected
  before recording the app's actual failing diagnostic decode assertions.
- [x] Static last-reset messages and a separate per-source output reset counter;
  receive-only TCP compatibility fix, unchanged timeouts/queues/media policy.
- [x] Native/app/sanitizer regression and new developer diagnostic bundle:
  10 native targets, 71 app tests, 10 ASan targets, 7 selected TSAN targets and
  7 policy tests pass (`build/obs-check-*.log`). Release candidate:
  `build/package-obs-diagnostics/Free PS5 Capture Card.app`. Bundle validation
  passes for 28 arm64 Mach-O files; strict recursive signature verification passes.
- [ ] User reproduces a drop and reports the last-reset message; root cause and
  actual remedy remain unproven until then.

Storage follow-up: [the current architecture](../architecture.md) supersedes
the historical Keychain persistence requirements in this continuation.
The current user-approved build uses an owner-only unencrypted file and starts
fresh, without accessing older Keychain records.

## Credential-file follow-up — 2026-11-14

User explicitly chose owner-only unencrypted storage and start-fresh setup,
then reconfirmed the exception after a concurrent policy edit restored the old
blanket prohibition. No old Keychain access, migration or deletion is permitted.
The separately committed file-storage plan was subsequently deleted outside
this implementation; that deletion is left unstaged rather than overwritten.

- Backend scaffolds failed first in `build/file-store-red.log`; the initial
  complete backend passed 65 app tests in `build/file-store-green.log`.
- Final `build/file-store-app.log`: 70/70 app tests pass, including private modes,
  owner/type/link/size/schema checks, nonregular/FIFO rejection, stale records,
  lock contention, corrupt reset, account/pairing preservation and no OAuth-secret
  persistence after a mocked successful sign-in.
- `build/file-store-native.log`: 10/10 native targets pass; native media unchanged.
- `build/file-store-policy.log`: 7/7 policy/entry-point checks pass, including no
  application credential-Keychain APIs/imports and no production codec/device use.
- Release bundle `build/package-file-store/Free PS5 Capture Card.app` passes the
  recursive 28-file runtime closure, architecture, minimum OS and signature checks
  (`build/file-store-package.log` plus fresh checker). Release imports no direct
  `SecItem*` functions, contains no old credential service ID or Debug UI-test flag.
- Default record is `~/Library/Application Support/FreePS5CaptureCard/credentials.json`,
  directory 0700 and file 0600. This is not encryption or protection from same-user
  processes. Normal writes use a private lock and bounded, checked atomic replacement.
- All real filesystem tests use unique temporary directories; other tests use
  injected fakes. Actual user credentials/Keychain/PSN/PS5 were not accessed.
- Existing GUI automation, real sign-in/pairing/relaunch with this new backend,
  media/sync/HDR/soak, clean-machine and independent review gates remain open.

## Goal and approved contract

Finish Tasks 6–10 from `free-ps5-capture-card.md` sequentially on `main` without
asking the user to approve each next step. The user explicitly deferred hands-on
PS5/OBS testing until the final handoff; automated red/green/regression checks
continue throughout. Pause only for a real decision/blocker. Do not report manual
HDR/sync/recovery or clean-machine packaging acceptance without actual evidence.

Native SwiftUI app + C/C++ CaptureEngine + pinned Chiaki core, compressed video
and Opus in live Matroska on one localhost TCP Media Source. No decoding,
encoding, preview, local sound, controller or microphone. H264, H265 default,
and H265 HDR required; preserve HDR or report negotiation mismatch, never fake
tags. Both tracks are mandatory. Keychain credentials already pair successfully
and the user confirmed relaunch recovery. Keep normal external-browser PSN login.

## Progress

- [x] Tasks 0–5 and first Task 6 batch committed through `34c1a43`.
- [x] Existing clock, queue and NAL/Opus units: 22 cases; full native 8 targets,
  app 41 tests; ASan/UBSan and queue/clock TSAN pass.
- [x] Finish Task 6: real codec metadata/cadence validation, bounded mux path,
  live Annex-B/Opus remux equivalence.
- [x] Task 7 implementation: live native session, Swift connection/options/status, both tracks
  through the pipeline; secure transient login passcode handling.
  Implemented native lifecycle and receiver pipeline, C/Swift configuration,
  sanitized status and masked transient passcode controls. Full native/app
  regression passes in `build/live-session-regression.log`; device gate deferred.
- [x] Task 8 implementation: transient retry, cancellation and reconnect/fault coverage.
  Delayed/missing audio, OBS reconnect, 100 raw TCP reconnects, second consumer,
  bounded stalls and backoff cancellation pass. Stable connection resets backoff.
  Stop clears the engine's copied credentials and requires reconfiguration.
- [x] Task 9 implementation: focused native setup/options/status UI, persistence and accessibility.
  Native utility direction, system controls, codec order/default, bitrate and port
  tests; secure passcode retry/cancel; disabled setup until Stop acknowledgement.
  Manual visual/VoiceOver inspection and XCUITest remain unpassed gates below.
- [x] Task 10 implementation: bundle checks/developer packaging, CI, setup docs and explicit
  review handoff; final user tests remain clearly listed.

### Current verification and remaining acceptance

- Latest ordinary native run: 10/10 CTest targets pass
  (`build/verified-native.log`).
- Fresh final ASan/UBSan: 10/10 targets; TSAN: 7/7 selected targets, including
  receiver/transport (`build/verified-asan.log`, `build/verified-tsan.log`).
- Final direct Debug XCTest: 47/47; reproducible explicit unit-only runner
  `scripts/test-app-unit-direct.sh`, log `build/verified-app.log`.
- Normal Xcode unit-host startup subsequently stalled in XCTest's IDE-session
  setup. Direct execution is not app-launch or UI acceptance. UI runner separately
  timed out enabling macOS automation. No permission/service changes were made.
- Final Release developer bundle: `build/package-handoff/Free PS5 Capture Card.app`.
  Recursive checker passes 28 Mach-O files with 26 copied dylibs, no external
  runtime references/rpaths, compatible arm64/minimum-OS metadata and signatures.
  Full Chiaki/static-library and formula notices are included. Direct compiled
  import checks find no codec-opening/decoding/encoding calls from the engine;
  the Debug UI-test launch flag is absent from the Release executable.
- Final inspection fixed missing process-wide core initialization before live
  session setup and preserved explicit registration/identity error codes instead
  of retrying them as generic connection failures. Red/green logs:
  `build/core-init-{red,green}.log`, `build/auth-rejection-{red,green}.log`.
  Eight-thread initialization and partial-initialization cleanup tests pass.
  The current locked sink patch SHA-256 is
  `177585e8b2281287730a2297775ece23595f363f8a6b35a0c8552e96c13cc125`.
  Fresh sequential application reproduces all changed core files byte-for-byte
  under `build/patch-verify-handoff`.
- Checkpoint commits: `99095c1` live session/pipeline; `a6f0b68` recovery/native UI;
  `271caa9` core initialization/identity failure fixes and loss/queue diagnostics;
  packaging/docs/CI closeout follows on `main`. Nothing was pushed.
- Real PS5/OBS testing, accurate sync/drift/HDR, 30-minute per-mode runs,
  controller/TV/offline/sleep behaviour, actual recovery/resource measurements,
  clean-machine launch and independent code/security/license review remain open.
  See `docs/validation.md` and the explicit `docs/review-handoff.md` request.

## Implementation and verification

1. `Native/src/LiveFormat.*` should inspect real SPS/VUI and first access unit via
   public parser APIs without opening decoder contexts, retain raw headers/SEI,
   and construct live stereo/48 kHz Opus parameters with pre-skip 0. Verify 30 fps
   content independently of requested 60 and reject unrepresentable reordering.
   Add tests and an Annex-B fixture path to `Tests/Support/integration.py`.
2. Connect `Timeline`, `PacketQueue`, `MatroskaMuxer`, `LocalTCPServer` using
   bounded borrowed-input admission. Account for worker/mux allocations as well
   as queued/leased data. Reconnect uses fresh Matroska headers and ONE epoch.
3. Extend `CaptureEngine.h` with copied connection credentials and rich sanitized
   status. Session worker owns Chiaki init/start/stop/join/fini. Reserve port
   before console contact. IDR access is gated by `SessionMediaControl`.
4. Extend `EngineClient`, `AppModel`, stream options/preferences and views. Stop
   before forgetting/replacing credentials; UI thread never joins native work.
5. Run focused `scripts/test.sh --native --filter ...` and `--app --only ...`,
   then `scripts/test.sh --native --app`. Build Debug/Release and verify signatures.
   Use separate ASan/UBSan/TSAN build directories for native concurrency tests.
6. Package under `build/package`, never Applications. Recursively bundle non-system
   dylibs/rpaths, ad-hoc sign inside-out and check all dependencies/architectures.
   Actual installed Homebrew libraries require macOS 26; do not advertise 15
   compatibility. No push/publish/notarization is included.

## Surprises & Discoveries

- `Engine.cpp` now owns asynchronous live lifecycle, reserves loopback before
  source contact, and uses real Chiaki init/start/stop/join/fini. No real PS5
  stream has been started by automated tests.
- Queue cap covers owned queue/config/leases, not total mux/AVIO/core RSS.
- Receiver-pipeline TCP tests now exercise borrowed callbacks, startup parsing,
  clock calibration, queue, live Opus initialization and ordered Matroska output
  for all three modes, decoded only by test-only FFmpeg.
- Pinned core receiver patch supplies source indices and first receipt times;
  nominal requested fps alone is not actual cadence or proof of HDR.
- FFmpeg parsers returned cadence/depth but not VUI colour; bounded native H264
  and HEVC SPS/VUI readers now preserve signaled colour and reject reorder.
- H264 fixture container tags differed from the encoded VUI; recipe 3 explicitly
  signals BT.709 in the encoder. No live source is retagged to hide missing VUI.
- Live Annex-B → Matroska tests pass for all modes, with identical VCL and Opus
  payloads, HDR metadata, actual 30 fps vs nominal 60, explicit mismatch/reorder
  rejection, and admission before an oversized packet's data is copied.
- Live queue uses a separate 3 MiB allowance, the core frame allocator is capped
  at 4 MiB, and ordered live muxing bypasses FFmpeg's interleaving queue. Source
  headers and packets are bounded; total RSS still includes non-media library/
  allocator/transport overhead and requires measurement at final live tests.
- HDR fixtures repeat identical VPS/SPS/PPS in the first access unit. Signatures
  now canonicalize/deduplicate parameter sets rather than treating repetition as
  a source change. Regression: `build/repeated-headers-red.log`, followed by
  passing TCP and full regression checks.
- Real session ownership is tested through injected core entry points, covering
  sink setup, no fallback/controller/keyboard flags, transient PIN, in-use quit,
  exactly-once cleanup, and start failure without joining an unstarted thread.
- Native UI now exposes persisted non-secret options, Start/Stop, actual packet
  counters/cadence, copyable URL and secure PIN sheet. Setup/options are disabled
  until Stop acknowledgement, so credentials cannot be forgotten mid-session.

## Decision Log

- User requested one continuous sequential implementation, automated checks along
  the way, hands-on tests only at the end. This explicitly defers Gate D as a
  development sequencing pause, not its evidence requirements for acceptance.
- Use checkpoint commits on `main`; do not create milestone feature branches.
- Reflect the installed dependencies' actual macOS 26 floor in developer build
  targets rather than leaving a misleading 15.0 declaration. This does not
  establish older-OS support or authorize a public release.
- Normal IDE/UI harness failures remain visible. The named direct Debug XCTest
  runner uses a minimal environment and tests unit code only, never silently
  converting a failed UI/app-launch run into a pass.
- Keep all remaining actual-device/clean-machine/review gates open, but hand off
  the implemented developer build now for the user's explicitly deferred tests.

## Outcomes & Retrospective

Implementation reached the final verified developer-packaging checkpoint. Native
session and receiver tests never contacted the user's console. User-deferred
hands-on gates, blocked GUI automation and independent review remain separate
from green unit/native/sanitizer evidence. No push, publishing, installation to
Applications, autostart, or notarization was performed.
