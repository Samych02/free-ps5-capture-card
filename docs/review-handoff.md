# Independent review requested

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
