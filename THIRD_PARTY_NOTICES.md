# Third-party components

## Chiaki and chiaki-ng

Original Chiaki: https://git.sr.ht/~thestr4ng3r/chiaki

Chiaki was originally developed by Florian Märkl and contributors. chiaki-ng
continues that work, maintained by Street Pea and contributors. Their Remote Play
implementation is the foundation of this app; we do not claim to have authored it.
This project is separate from, and does not imply endorsement by, either upstream.

Source: https://github.com/chiaki-ng/chiaki-ng

Pinned revision: `43728af2e639fecb8a7af1cd68c29820a40de973`.

The library is reused under its original AGPL-3.0-only license terms and applicable
OpenSSL exception text, preserved in `LICENSES/`. Original source notices and
submodule licenses remain in the fetched source. The initial patch only respects
an explicit embedding project's deployment target; it does not change protocol
or licensing behavior.

The native PSN account-identification flow adapts URL/client configuration and
token/account-ID exchange behavior from `gui/include/psnaccountid.h`,
`gui/src/psnaccountid.cpp`, and `gui/src/psntoken.cpp` at the same pinned revision.
The original account-ID code credits grill2010's script. New native code retains
the project's AGPL terms, does not copy Qt settings/UI dependencies, and adds
strict redirect/state validation and sanitized error handling.

`patches/chiaki/0002-encoded-media-sink.patch` adds a separate encoded-media
consumer to the pinned core, receiver metadata, audio callbacks outside receiver
locks, and guarded IDR transport lifetime. It preserves legacy callback signatures
but changes internal/public core struct layouts; no binary ABI compatibility is
claimed. A regression fixes the core's failed-video-frame loss count across a
16-bit frame-index wrap. H264/HEVC header and slice bytes used in
`Tests/Native/MediaSinkTests.cpp` come from the pinned upstream `test/bitstream.c`
under the same AGPL/exception terms. This work does not add a media decoder.
The patch also bounds core frame/header allocation and preserves explicit server
account/registration rejection codes as typed nonretryable identity failures.

`patches/chiaki/0003-control-frame-bounds.patch` validates received control frame
lengths against the fixed buffer before addition/decryption, then uses one checked
size for consumption and buffer movement. It preserves the upstream license and
valid negotiation behavior. Existing controller/microphone negotiation is
intentionally unchanged pending later real-console compatibility testing; this
does not add microphone capture or controller features to the app.

## Build/runtime dependencies

FFmpeg, OpenSSL, curl, nanopb, MUnit, gf-complete/Jerasure, json-c, miniupnpc,
libevent, protobuf and build tools retain their respective licenses. No binaries
are published at this stage. Local developer bundles enumerate recursive linked
libraries in `Contents/Resources/RuntimeNotices/runtime-manifest.json` and include
top-level formula license texts/build recipes alongside this project's notices.
Independent license compatibility and source-obligation review is still required
before redistribution; actual Homebrew FFmpeg options matter.

The earlier RC1/audit candidate uses FFmpeg 9.0.2 (`--enable-gpl --enable-version3`),
licensed GPL-3.0-or-later, with x264/x265 and other transitive libraries. Not using
an encoder/decoder does not remove the licensing obligations of the linked binary.
The statically linked curl submodule uses the curl copyright/permission license;
its `COPYING` must accompany copies. Packaging now includes curl alongside
Chiaki, gf-complete, Jerasure and nanopb notices. Earlier RC1 omitted curl's notice
and must not be redistributed unchanged.

Current optimized builds instead statically link a SHA-256-pinned source build
of FFmpeg 9.0.2 with no GPL/version3 option, external codec libraries, registered
encoders/decoders, demuxers or protocols. Required H264/HEVC parsers and Matroska
output remain. A distinct fixture-input profile is test-only and never packaged.
The complete upstream license texts, actual configuration/source manifest and
build recipe are included under `ProjectNotices/EmbeddedSources/ffmpeg`.
The smaller FFmpeg configuration does not relicense the AGPL project or erase
static-linking/corresponding-source obligations. Do not use old candidate GPL
metadata to describe the new runtime or vice versa.

See [source-distribution requirements](docs/source-distribution.md) for the actual
candidate dependency inventory, evidence locations and unresolved release gates.
The `EmbeddedSources` resource directory contains **notices only**, not complete
source trees. Neither that directory nor upstream hyperlinks constitute a
completed corresponding-source distribution.

New project code is licensed under AGPL-3.0-only. This notice is not a claim that
an arbitrary third-party binary configuration has passed redistribution review.
