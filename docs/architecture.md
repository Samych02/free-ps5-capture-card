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
Fresh Remote Play in-use rejection remains nonretryable. Only an automatically
restarted, previously established session within the same capture invocation
arms a 15-second release grace period after full teardown. In-use replacements
may retry within it, with waits capped at 2 seconds; failed replacements cannot
refresh the window. A successful connection clears the old release context and
Stop cancels backoff. No takeover or remote-disconnect commands are sent.

The patched pinned core supplies compressed video/Opus, source indices, receipt
timestamps, configuration generations and loss/recovery metadata. `Pipeline.cpp`
checks bounds/startup metadata and forwards leased queue packets into FFmpeg's
public Matroska mux API. Parsers run without opening decoders. SPS/VUI nominal cadence, colour
and reorder constraints govern admission; HEVC static HDR SEI is retained and
changes trigger source reconnection.

Audio PTS advances by exact validated Opus samples with a frozen initial phase.
Live video PTS uses first-fragment ingress times mapped onto that audio sample
timeline: a fixed 16×25 ms bucket lower envelope of valid audio receipt-minus-media
delay relates the shared monotonic ingress clock to the audio clock. Missing
markers do not train the estimator; payload gaps retain its last valid reference
while existing track-gap/reset rules apply. Video PTS is immutable at admission;
equal/backward receipt times clamp forward by 1 us. Raw indices still provide wrap,
duplicate, reordering and discontinuity checks, but SPS FPS is not treated as the
transport frame-index clock. A counter-based strategy remains for tests with a
verified counter clock; live selection is explicit, never a runtime fallback.
No encoded frame is decoded, resampled, duplicated or dropped by this mapper.
Ingress jitter is not an encoder timestamp; measured A/V sync remains a gate.

Both tracks are mandatory. Each OBS connection gets fresh Matroska state and a
common epoch, not independent per-track zeroes. Source reconnection destroys the
old queue/clock/session first. No consumer means discard+accept, not pretend
decoder failure. Track/configuration/keyframe waits are bounded. Ordered live
muxing bypasses FFmpeg's separate interleave queue.

The live owned queue allows 3 MiB, with separate bounded startup/parser, packet,
core-frame (4 MiB), header (64 KiB), cluster and AVIO storage. This is not a process
RSS limit. Library/allocator/transport overhead and growth require measurement.
Socket writes are nonblocking with a 500 ms stall budget; second consumers close.
Receive EOF on the client's unused sending half is not treated as proof that its
receiving half closed. Read-only peers remain valid; write errors/timeouts detect
closed or nonreading consumers under the existing bounds.
Low-frequency status includes both packet counts, source loss/hole counts, queued
bytes/oldest queued-packet age, actual encoded dimensions/cadence and retries.
Queue age excludes an in-flight lease and is not end-to-end latency.
OBS output resets have a separate per-source counter and a retained sanitized
reason using fixed text and bounded numeric diagnostics. A client closing, a bounded write stalling, and a queue reset
are not conflated with a PS5 session retry. Fatal source errors take precedence
over this history. No raw socket/protocol strings or media payloads are logged.
Missing-track resets preserve configuration/startup/delivery stage, per-track
receipt ages and any startup audio-to-keyframe PTS delta before clearing state.
This snapshot does not relax queue policy or represent an end-to-end sync measure.
Per-source clock diagnostics compare each accepted unwrapped counter's nominal
media duration with elapsed first-to-last receipt time, independently of the
frozen phase estimate. Both signed drifts, counter spans and the audio sample
duration reach status; UI reports observed counter rates separately from SPS FPS.
These diagnostics remain nominal counter measurements, not corrected output-clock
error; negative nominal video drift can remain visible while output is healthy.

The app never opens a codec, plays media, initializes controller/microphone media,
tone-maps or records. OBS decodes both tracks and optionally encodes final output.
Decoder/encoder symbols may exist inside bundled FFmpeg; their presence does not
mean the app calls them. Source checks and independent review cover that boundary.

See the [design contract](plans/free-ps5-capture-card.md),
[living continuation](plans/finish-live-capture.md) and [validation](validation.md).
Receipt-offset calibration is not proof of real source A/V phase or long-term sync.
