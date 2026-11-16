# Independent review requested

Latest scope/evidence: [capture-only policy](reviews/capture-only-negotiation.md).
Review the fourth locked patch's input suppression, preserved media paths, feedback
lifetime and enabled app opt-in. The user approved this new developer candidate,
superseding the earlier deferral; actual PS5 compatibility is still unverified.
Also review the [offline audit fixes](reviews/offline-audit-fixes.md): parser patch,
cached-source enforcement, C API v3 PIN identity and descriptor ACL validation.
Hosted CI and PS5 tests remain pending.

## Scope

Review live capture changes from `6a53f90` through the final continuation commits
on `main`, including `99095c1` and the recovery/UI/packaging work. No remote push,
PR, merge or release has been performed. This document requests review; it does
not represent an independent approval.

## Requirements to verify

- Compressed video and stereo/48 kHz Opus are equally mandatory, in one live
  Matroska/TCP stream. No app codec opening, decode/encode, playback, preview,
  controller/microphone devices, tone mapping or hidden HDR fallback.
- Credentials use an explicitly approved, unencrypted owner-only file or bounded
  session memory; PINs/OAuth redirects/tokens are transient. Review private modes,
  ownership/link/type checks, size limits, expected-byte writes under advisory
  lock, atomic replacement, corrupt-record reset and temporary-file cleanup.
  No old Keychain access/migration/deletion is permitted. Raw core logs/remote
  errors never reach diagnostics. Same-user process access is an accepted risk.
- Reserve the configured loopback port before console contact. No automatic port
  switching, concurrent old/new session owners, callback-after-free or UI joins.
- Verify teardown order, IDR gate, cancellation, partial init/start failures,
  stale generation/consumer epochs, shared-clock calibration and no-reorder proof.
  Check once-only global core initialization before session setup, typed explicit
  registration rejection mapping, and credential-memory clearing on Stop.
- Audit every allocation owner: startup copy, parser, leased queue, packet, core
  frame, AVIO and Matroska cluster. Queue allowance is not a process-RSS promise.
- Test whether actual PS5 SPS/VUI/SEI and Opus metadata satisfy admission. Synthetic
  fixtures cannot establish live source phase/drift or exact HDR output.
- Check masked PIN retry/cancel behaviour, setup/options disabled through Stop
  acknowledgement, non-secret persistence, copyable URL and shared window state.
  Loss/hole counters and oldest queued age are diagnostics, not a claim of
  measured end-to-end timing or process memory bounds.
- Inspect recursive bundling, rpaths, architectures, minimum OS, ad-hoc signing,
  license copies and actual FFmpeg/OpenSSL compatibility/source obligations.
- OBS-drop follow-up: review receive-only TCP half-close support, full peer-close
  and stalled-write cleanup, per-source reset counting and reason precedence when
  a queue reset aborts a write. Fixed message text and numeric reset snapshots must
  stay free of protocol strings and credentials. The user reproduced MissingTrack
  after the TCP fix; distinguish startup alignment from actual delivery gaps.
  The user-specific live-drop root cause is not yet confirmed.
- Clock follow-up: inspect the distinction between SPS nominal cadence and
  measured counter progression. Diagnostic drift is media-duration minus elapsed
  receipt time, not measured A/V sync; per-track spans must survive counter wrap,
  ignore rejected duplicate/reordered packets and remain separate from OBS epochs.
  The measured live video mismatch is now addressed with an explicit audio-anchored
  ingress-video strategy. Review audio-delay bucket aging and missing-payload
  handling, immutable/monotonic video PTS, overflow bounds, continued exact audio
  sample PTS and OBS reconnect continuity. The correction uses ingress timing,
  not encoder PTS; verify real A/V sync and jitter behaviour independently.
- Transition recovery: review the owner-scoped console-release grace. Fresh busy
  must remain terminal; only full teardown of an established automatic-retry
  attempt arms the window. Busy/failed attempts must not extend it, reconnection
  clears stale context, and Stop must prevent another attempt. Confirm no takeover
  commands or HDR fallback. The user's Stop/wait/Start result supports a release
  delay, not proof that HDR metadata was the initiating event.

## Evidence and limitations

### 0.1.0 release-candidate UI/status review

Review changes after `2b345f1` in `CaptureEngine.h`, `Engine.cpp`, `Pipeline.cpp`,
`App/EngineClient.swift`, `App/AppModel.swift`, `CapturePresentation.swift`,
`PairingModel.swift`, all views, the single-window app/delegate and related tests.
Expected: typed current phases and separate bounded history, no false streaming
while paused/blocked, no transient terminal-failure flash during auto retry,
consistent asynchronous snapshot/state handling, strict visible port/pairing-code
validation, duplicate-action guards and safe PIN dismissal/cancellation.
Keep credential edits locked through Stop acknowledgement; inspect conditional
disabled states and focus/cancellation in account, pairing, address and PIN sheets.
The single window must close/reopen without stopping an active capture, while
Quit still waits for cleanup. Inspect actual fake-data SwiftUI screenshots in
`build/ui-polish`; no user credential files are involved in those renderings.
Check that C API v3 layout/version and bundled Swift client/framework match,
including typed PIN request IDs for repeated identical rejections.

No independent review pass is claimed by the author. Automated, rendering and
manual/release evidence must remain separate. Public distribution is not
authorized. Remaining gates are explicit in `docs/release-checklist.md`.

### Protected-content crash review scope

Review display-sink registration in `Session.cpp` and `setSourceBlocked` in
`Pipeline.cpp`/`Pipeline.h`. The pinned core unconditionally calls cantdisplay_cb
from RP-Prohibit connect handling and live restriction messages; macOS crash
reports show null calls at those control-thread paths. Sink registration must
precede start, and its pipeline context must outlive core join. Verify blocked
packets cannot reach OBS, startup timers do not fail an explicitly blocked
session, Stop/core-quit remain authoritative and unblock requires fresh A/V.
No decoder, playback or restriction bypass is introduced. Native lifecycle tests
invoke blocked before CONNECTED; integration sends valid HDR A/V throughout a
blocked interval (including initial block >5 s) and asserts it is discarded,
then checks post-unblock two-track decode. Live media-app confirmation and
independent release review remain pending.

### HDR media-gap recovery review scope

Review the media-gap follow-up in `Pipeline.cpp`, retry-reason retention in
`Engine.cpp`, and `test_pipeline_recovers_long_media_gaps_without_source_restart`.
Live evidence identifies the old two-second media watchdog as the restart trigger.
Check that source silence cannot tear down Remote Play, old output clocks/packets
are discarded, both tracks must be freshly observed before recalibration, and
source silence retains bounded state without preventing Stop or core-quit handling.
This is output-epoch recovery, not preservation of audio timestamps across a gap
with unknowable source timing. Compatible header-only changes must retain their
existing continuity behavior. Also check that owned-session busy retries do not
erase the initiating source failure; PIN/terminal errors must remain authoritative.
Independent review and actual-device acceptance remain deferred release gates.

### In-place video transition review scope

Review `Pipeline.cpp`, `PacketQueue.cpp`, `Timeline.cpp`, `LiveFormat.cpp` and
`SPSMetadata.cpp` plus their transition tests. Supported updates must not produce
a source-done/retry result. Compatible generations and dynamic HDR SEI must keep
OBS connected; genuine parameter-set changes may reset only output. Audio sample
PTS must stay continuous, video PTS immutable, and obsolete generations/parser
results/reordered SPS must not overwrite current metadata. Inspect bounded PS
type/ID merging, static HDR SEI serialization/precedence for late decoder joins,
and the internal parser test seam. No C API/user-selectable parser bypass exists.
Latest native evidence: `build/inplace-native-final.log`; additional verification
logs use `build/inplace-check-*`. Real transitions and OBS-specific HDR behavior
still require independent acceptance; synthetic FFmpeg decoding is not that gate.

See [validation](validation.md) for command logs, automated results and explicit
pending gates. Native ASan/UBSan passed; UI automation could not initialize on
the development desktop. Basic real pairing and synthetic OBS ingestion are
user-confirmed; live PS5/OBS timing/HDR/reliability are deliberately deferred.
Clean-machine launch and legal/security approval remain outstanding.

Please return findings by severity with file/line references and a reproducer.
Blocking findings must be fixed and affected tests rerun before release.
