# Capture architecture

SwiftUI owns account/pairing UX and non-secret preferences. A 0600 unencrypted
file in a 0700 application directory stores one versioned account+console record.
Browser OAuth material, pairing codes and login
passcodes are transient; protocol logs are disabled.

`FileCredentialStore` validates schema, account and console binding, while
`PrivateCredentialFile` bounds reads to 64 KiB, checks ownership/private modes and
regular-file/link types, refuses symlinks, and uses descriptor-relative I/O.
Cooperating instances use a nonblocking private advisory lock and compare the
expected record bytes before mutation. Writes use a new exclusive 0600 temporary
file followed by flush/atomic rename; normal failure cleans temporary data.
Stale edits fail instead of restoring removed/replaced credentials.

This is not encryption and does not protect against processes running as the
same user. OS/user backups may retain copies; deletion is not secure erasure.
There is no Keychain backend, fallback or migration, and old Keychain records
are untouched. The start-fresh file design explicitly supersedes the earlier
Keychain-only design in historical plans/reviews.

`CaptureEngine.framework` exposes a versioned C API. Its worker reserves the
chosen IPv4 loopback listener **before** console contact, owns copied credentials,
and serializes start/stop/retry/PIN commands. `Session.cpp` owns Chiaki init,
sink installation, start, stop, join and fini. Callbacks never join their session.
The IDR gate closes before transport teardown; borrowed callback context outlives
core receive-thread joins.
Live setup first initializes Chiaki's process-wide FEC tables/certificate-cache
mutex exactly once, serialized across engine instances. Explicit server account/
registration rejections retain a typed nonretryable identity error rather than
being collapsed into an indefinitely retried connection failure.

The patched pinned core supplies compressed video/Opus, source indices, receipt
timestamps, configuration generations and loss/recovery metadata. `Pipeline.cpp`
checks bounds/startup metadata, builds a rational timeline, freezes least-delay
source offsets and forwards leased queue packets into FFmpeg's public Matroska
mux API. Parsers run without opening decoders. Actual SPS/VUI cadence, colour
and reorder constraints govern admission; HEVC static HDR SEI is retained and
changes trigger source reconnection.

Both tracks are mandatory. Each OBS connection gets fresh Matroska state and a
common epoch, not independent per-track zeroes. Source reconnection destroys the
old queue/clock/session first. No consumer means discard+accept, not pretend
decoder failure. Track/configuration/keyframe waits are bounded. Ordered live
muxing bypasses FFmpeg's separate interleave queue.

The live owned queue allows 3 MiB, with separate bounded startup/parser, packet,
core-frame (4 MiB), header (64 KiB), cluster and AVIO storage. This is not a process
RSS limit. Library/allocator/transport overhead and growth require measurement.
Socket writes are nonblocking with a 500 ms stall budget; second consumers close.
Low-frequency status includes both packet counts, source loss/hole counts, queued
bytes/oldest queued-packet age, actual encoded dimensions/cadence and retries.
Queue age excludes an in-flight lease and is not end-to-end latency.

The app never opens a codec, plays media, initializes controller/microphone media,
tone-maps or records. OBS decodes both tracks and optionally encodes final output.
Decoder/encoder symbols may exist inside bundled FFmpeg; their presence does not
mean the app calls them. Source checks and independent review cover that boundary.

See the [design contract](plans/free-ps5-capture-card.md),
[living continuation](plans/finish-live-capture.md) and [validation](validation.md).
Receipt-offset calibration is not proof of real source A/V phase or long-term sync.
