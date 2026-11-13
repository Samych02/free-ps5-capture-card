# Finish standalone PS5 capture

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
- [ ] Task 10: bundle checks/developer packaging, CI, setup docs and explicit
  review handoff; final user tests remain clearly listed.

### Current verification and remaining acceptance

- Latest ordinary native run: 10/10 CTest targets pass
  (`build/final-native-app-rerun.log`).
- Fresh final ASan/UBSan: 10/10 targets; TSAN: 7/7 selected targets, including
  receiver/transport (`build/final-asan-tests.log`, `build/final-tsan-tests.log`).
- Final direct Debug XCTest: 47/47; reproducible explicit unit-only runner
  `scripts/test-app-unit-direct.sh`, log `build/direct-unit-script.log`.
- Normal Xcode unit-host startup subsequently stalled in XCTest's IDE-session
  setup. Direct execution is not app-launch or UI acceptance. UI runner separately
  timed out enabling macOS automation. No permission/service changes were made.
- Developer bundle implementation and recursive/signature validation exist;
  final refreshed bundle is being assembled under `build/package-final`.
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

## Outcomes & Retrospective

Implementation has reached the final verification/packaging checkpoint. Native
session and receiver tests never contacted the user's console. User-deferred
hands-on gates, blocked GUI automation and independent review remain separate
from green unit/native/sanitizer evidence. No push, publishing, installation to
Applications, autostart, or notarization was performed.
