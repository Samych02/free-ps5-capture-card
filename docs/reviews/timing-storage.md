# Review request: Task 6 timing and owned-storage batch

Scope: changes since `c881c1d`: Timeline, PacketQueue, AnnexB and AudioFormat,
their tests, build/test entry points and the synthetic-clock integration case.
This is a **partial Task 6 checkpoint**, not completed live remux or a session.

## Behavior

- Caller-verified rational video cadence and fixed audio samples/rate map
  independent 16-bit source indices onto one monotonic media origin. A 250 ms
  observation window freezes each track's least-delay estimate. Audio gaps
  retain their source position/sample duration; explicit user offset is applied
  before a consumer chooses one common epoch. OBS reconnect never re-anchors
  the clock. There is no resampler, arbitrary PTS nudge, or decoder.
- The clock is caller-serialized and begins at media stream-info, not a slow
  authentication/network handshake. The caller freezes at the deadline before
  post-window observations. Missing tracks, invalid samples, ambiguous/implausible
  counter jumps and long no-progress intervals require a new source clock.
- The queue owns borrowed bytes only after admission under its mutex. Fixed
  metadata slots, 256 total logical entries, 16 MiB owned-storage budget, 64 KiB
  header and 4 MiB packet bounds cover headers and in-flight shared leases. An
  allowance accounts for shared-control metadata; this is not allocator RSS.
  Snapshots clear borrowed pointers and include a consumer epoch. Their buffers
  can outlive queue reset/destruction without being prematurely uncharged/freed.
- No consumer means discard media without copying; only current bounded
  configuration remains. Before a random-access picture, keep a bounded rolling
  audio pre-roll and discard predicted video. Never announce ready with stale or
  missing audio. First arriving headers are initialization, not replacement.
- Overflow, stale live media, bad timestamps, changed config and missing tracks
  invalidate both tracks and the consumer epoch. Reattach requires a new random
  access point; no predicted-frame continuation after a dropped dependency.
  A source session owns a fresh queue/clock; do not reuse raw generation IDs for
  a different PS5 session. The output worker must act on reset epochs, close the
  old mux/TCP connection and reject stale snapshots/packets.
- Pop keeps one-track look-ahead; an explicit finite-stream drain can bypass
  that. The output owner must continue maintenance when no callback arrives.
- NAL-header classification distinguishes H264 IDR from a non-IDR I-slice and
  HEVC BLA/IDR, CRA and leading pictures. Mixed/inconsistent/malformed access units
  are rejected without decoding. This is NOT an SPS/VUI cadence/HDR parser.
- Live OpusHead uses stereo/48 kHz and pre-skip 0, never synthetic encoder delay.
  The TOC helper returns RFC 6716 duration; it is not a full Opus payload validator.
  Live receiver/mux integration must validate it against the wire sample count.

## Evidence and review focus

Observed red tests preceded each implementation: empty timeline, queue, codec
helpers; first-config handling and bounded pre-roll; epoch-tagged configuration;
fresh counter reset incorrectly ignored as reordering; and stale pre-roll
incorrectly marking the queue ready. All have green regressions.

The batch adds 22 native cases: 5 timeline, 12 queue, 5 codec-unit. The two-hour
synthetic test covers independent multi-wrap counters and exact rational/sample
progression. Queue tests cover concurrent producers, overflow without predicted
continuation, bounds before copying an invalid pointer, leases surviving owner
destruction, and invalid/changed/stale generations.

Fresh verification: `scripts/test.sh --native --app` passes 8/8 CTest targets
and 41/41 XCTest cases; bootstrap 4/4 passes. The `synthetic-clock` integration
case passes. ASan/UBSan with halt-on-error passes 10 repeats of all three new
targets; TSAN with halt-on-error passes the 12 queue and 5 timeline cases using
`--no-fork`. Debug/Release builds and ad-hoc signature checks pass. No reported
sanitizer error was ignored or suppressed.

Review logical storage accounting and lease lifetime, callback/control-thread
ownership, calibration deadline, reset propagation, common-epoch normalization,
and the requirement for verified cadence. Independent review remains pending.

## Explicitly incomplete

Task 6 still needs actual codec cadence/reordering and HDR/SEI/colour parsing,
live Annex-B/Opus container initialization/equivalence, and total mux/AVIO/cluster
buffering validation. The existing synthetic Matroska replay is NOT wired through
this queue. The queue cap alone cannot prove the entire output path is bounded.
Actual PS5 A/V phase, drift and HDR output are manual live gates. Start Capture
remains disabled; no user-console session was started by this work.
