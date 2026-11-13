# Review request: encoded-media receiver boundary (Task 5)

Scope: changes since `d9c35b5`: the second locked Chiaki patch, owning native
EncodedMedia adapter/SessionMediaControl, real-receiver tests and build wiring.
No live session is started and no user credentials are used by these tests.

## Contract

- Alternate pre-start consumer, not a tee. All four typed callbacks are required.
  Installation and start both reject conflicts with legacy game video/audio
  sinks. Legacy callback signatures and no-alternate-sink delivery remain intact.
- Configuration precedes samples; video profile changes/audio headers advance
  per-track generations. Raw headers retain available VUI/SEI/color information.
  Effective codec selection and nominal requested fps are surfaced, not claimed
  as independent verification of actual codec/cadence or HDR.
- Receiver-entry monotonic times and raw 16-bit frame indices survive video
  assembly/FEC and audio jitter buffering. Missing audio is an explicit hole with
  a sample count, never a zero-size Opus packet to mux. FEC and reference rewrite
  flags preserve the underlying core's recovery behavior.
- The consumer copies borrowed buffers, caps headers at 64 KiB and packets at
  4 MiB before allocation, clears borrowed pointers in owned records, and catches
  allocation/consumer exceptions at the C callback boundary. With no consumer it
  discards/accepts rather than inventing a decoding failure. This is not yet the
  combined queue/mux storage budget; Task 6 must enforce those bounds.
- Audio callbacks run after releasing the receiver mutex. Video true/false
  acceptance still controls core reference tracking. A video flush uses its old
  profile before a new profile configuration is emitted.
- IDR sends hold a dedicated transport gate; Stop/close disables it before
  Takion teardown. SessionMediaControl serializes request vs stop/detach; its owner
  closes that gate before join/fini. No receiver callback joins anything.

## Evidence and review focus

Observed red runs precede the new delivery, conflict validation, owning adapter,
and lifecycle behavior. A failed video frame crossing the source counter wrap
reproduced a loss count of -65534 instead of 2; the sequence-arithmetic fix now
passes the real FEC-failure path.

`scripts/test.sh --native --app`: 5/5 CTest targets and 41/41 XCTest cases pass.
There are 14 new media tests plus unchanged 118 upstream, 3 interface, 5 pairing,
and synthetic transport coverage. Bootstrap 4/4 passes. Debug/Release builds and
signatures pass. Fresh fetched source matches all 11 patched files byte-for-byte
and passes all native tests; the media target passes 10 ASan/UBSan repeats.

Review callback ordering, concurrent shutdown, IDR gate lock order, borrowed data,
error propagation, and legacy/feature-disabled regressions. Controller inspection
found/fixed the wrap loss count and confirmed the old-profile flush ordering;
no independent code/security review has been completed.

Still pending: common source timeline, actual cadence/reordering/color parsing,
combined storage caps/queue reset behavior, mux integration, live PS5 A/V, and
real measured synchronization/HDR validation. The HEVC-mode test uses an upstream
SDR fixture for enum/payload passthrough, not fabricated proof of HDR. Stop must
remain serialized with finalization by the owning app lifecycle. A changed core
struct layout is source-level integration, not a stable binary ABI promise.
