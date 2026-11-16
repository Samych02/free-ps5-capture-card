# Licensing and corresponding source before distribution

## Current status

**Public binary distribution is blocked pending source assembly and independent
license review.** This is an engineering inventory, not legal advice or a license
compatibility certification. Nothing here authorizes publishing or uploading.

Project code remains AGPL-3.0-only under [LICENSE](../LICENSE). Preserve upstream
copyrights and the [Chiaki OpenSSL exception](../LICENSES/AGPL-3.0-only-OpenSSL.txt)
where applicable; do not silently replace it or extend it to unrelated code.
GPLv3 and AGPLv3 have provisions for combined works, but each dependency's actual
license/version and any additional conditions still need review.

## Source and modification provenance

- [Dependencies.lock.json](../Dependencies.lock.json) pins chiaki-ng revision
  `43728af2e639fecb8a7af1cd68c29820a40de973`, selected submodules and patch hashes.
  Preserve the submodule revisions from that upstream tree as well as their files.
- [0001](../patches/chiaki/0001-embeddable-build.patch) respects the embedding
  project's explicit macOS deployment target.
- [0002](../patches/chiaki/0002-encoded-media-sink.patch) adds encoded-media sinks,
  receiver timing/generations, bounds, callback/IDR lifetime safety and typed
  rejection fixes. Upstream struct layouts change; this is a modified core.
- [0003](../patches/chiaki/0003-control-frame-bounds.patch) rejects oversized and
  wrapping control-frame lengths before decryption and guarantees bounded parser
  progress with one checked byte count. Preserve this patch and its lock digest
  when assembling matching source for the offline-audit build.
- [0004](../patches/chiaki/0004-capture-only-input-policy.patch) adds explicit
  capture-only input suppression and feedback lifetime guards, enabled in app
  build 4. Include it and its lock digest in matching source; upstream core
  struct layouts change and real-console acceptance remains unverified.
- [0005](../patches/chiaki/0005-capture-only-control-handshake.patch) extracts the
  bounded initial control request formatter. Its original build-5/6 revision
  omits both controller HTTP declarations; the revised build-7 digest retains
  ControllerType and omits only PadProcNo as an isolated experiment. Match the
  exact lock/commit, not just patch filenames, when assembling source. Build-4
  source retains its four-patch provenance.
- The app's PSN flow adapts chiaki-ng account-ID/token code. Retain its credit to
  grill2010. [Third-party notices](../THIRD_PARTY_NOTICES.md) and the README credit
  original Chiaki/Florian Märkl and chiaki-ng/Street Pea and contributors.

## Inspected dependency inventory

### Current minimal-FFmpeg follow-up

Current builds replace the Homebrew FFmpeg runtime with private static FFmpeg
9.0.2, pinned by source archive SHA-256 in `Dependencies.lock.json`. Runtime and
test-only fixture profiles are separate; no fixture-input/decoder/encoder/network
capabilities are added to CaptureEngine. Build configuration, compiler/SDK/source
identity and upstream licenses are copied to `EmbeddedSources/ffmpeg`; the verified
source archive is retained locally at `build/ffmpeg-9.0.2.tar.xz`. It is **not**
included in the app merely by copying its license files. A complete matching
source distribution and independent review remain required, including static
link/relink obligations. The project stays AGPL-3.0-only.

The inventory below is historical RC1 evidence, not the new dependency closure.
Re-inventory the optimized package before distribution. See
[portable foundations](portability.md) for exact profile boundaries.

Local optimized developer-package evidence: runtime policy reports **LGPL version
2.1 or later**, zero registered codecs/demuxers/protocols, two parsers and one muxer.
The app's dynamic manifest now contains **13 libraries** from json-c, miniupnpc,
libevent, libidn2, OpenSSL 3 and 4, libnghttp2, libpsl, libssh2, libunistring and
gettext. The two OpenSSL closures are still present; this work does not claim all
Homebrew dependencies are minimized. No FFmpeg/x264/x265/other codec dylibs remain.
Exact local package/check evidence is tracked in the
[execution plan](plans/minimal-ffmpeg-portability.md).

### Historical RC1 inventory

These are the **local RC1** versions, not promises for future Homebrew builds.
Evidence: the app's `Contents/Resources/RuntimeNotices/runtime-manifest.json`,
the copied formula `.rb` recipes and license texts, plus the fetched upstream
licenses. The manifest lists 26 dylibs from the formulae below. Formula metadata
is a starting point; full source/file notices govern the shipped components.

| Component | Observed version / provenance | License evidence / review scope |
| --- | --- | --- |
| Chiaki-ng | pinned revision above, statically linked | AGPL-3.0-only with applicable upstream OpenSSL exception |
| curl | pinned Chiaki submodule, statically linked | curl copyright/permission notice, `third-party/curl/COPYING` |
| gf-complete / Jerasure | pinned Chiaki submodules, statically linked | BSD-3-Clause-style `COPYING` notices |
| nanopb | pinned Chiaki submodule, statically linked | zlib-style `LICENSE.txt` |
| FFmpeg | 9.0.2_1 | GPL-3.0-or-later; `ffmpeg -L` reports `--enable-gpl --enable-version3`, no `--enable-nonfree` flag shown |
| x264 / x265 | r3222 / 4.3 | GPL-2.0-or-later; review GPLv3 selection in this combined build |
| OpenSSL | 3.5.9 and 4.1.0 | Apache-2.0; two major runtime closures are present |
| json-c / libnghttp2 / libpsl | 0.19 / 1.71.0 / 0.23.3 | MIT; preserve bundled data notices too |
| miniupnpc / libevent / libssh2 | 2.3.3 / 2.1.13_1 / 1.11.1_7 | BSD-3-Clause per recipes; inspect complete notices/patches |
| dav1d / libvpx / opus / SVT-AV1 | 1.5.4 / 1.17.0 / 1.6.1 / 4.3.0 | BSD-family; SVT also ships a BSD-2-Clause notice |
| LAME | 4.0 | LGPL-2.0-or-later per recipe |
| mpg123 | 1.33.7 | LGPL-2.1-only per recipe |
| libunistring | 1.5 | GPL-2.0-or-later OR LGPL-3.0-or-later per recipe |
| libidn2 | 2.3.8 | mixed GPL/LGPL/Unicode notices; assess the linked library and bundled tables, not the package as one grant |
| gettext | 1.0 | mixed GPL/LGPL terms; assess linked libintl and notices |
| xz | 5.8.4 | mixed terms; assess linked liblzma and all relevant notices |

MUnit and synthetic codec fixtures are test-only. Protobuf/generator Python,
CMake, Ninja, XcodeGen and Xcode are build tools, not automatically app runtime
dependencies. System frameworks/libraries are distinct from copied Homebrew
libraries. Re-inventory each candidate; do not treat this table as an exhaustive
file-level bill of materials or vulnerability scan.

## Required release work

1. Freeze the exact app revision and capture an immutable binary inventory with
   hashes, dependency versions, submodule commits, patch hashes, compiler/build
   configuration and FFmpeg configuration/license output. Homebrew prerequisites
   are **not** a reproducible binary lock.
2. Assemble and verify complete corresponding source for the combined covered
   work: this repository, patched Chiaki, required submodules and applicable
   non-system linked dependencies. Include the scripts/configuration/patches
   needed to build and install it. Preserve modification notices; ensure dates
   and identification of changes meet applicable license requirements.
3. For Homebrew-derived libraries, retain exact source archives or commits,
   formula revisions, applied patches/resources and build instructions matching
   the binary. Recipes/URLs alone do not prove source completeness or continued
   availability. Review any LGPL relinking/replacement obligations for the chosen
   distribution arrangement and any installation-information requirements.
4. Choose and implement a license-permitted source-delivery method. For a download
   release, provide equivalent, clear access to the exact corresponding source
   alongside the binary and test retrieval/build from a clean environment. Do
   not invent a written source offer without the ability to honor its terms.
   Review AGPL network source-offer requirements where applicable as well.
5. Audit **all** static and dynamic notices and copyright/data files, not just
   filenames matched by the current packager. Review the actual GPL/AGPL/OpenSSL
   combination and any codec patent/distribution issues for the intended use.
   Acknowledgment is not permission to relicense dependencies.
6. Obtain independent license/code/security review and close the
   [release checklist](release-checklist.md), including device/clean-machine
   evidence and separately authorized signing/distribution choices.

`package-dev-app.sh` copies notices and recipes; it does **not** implement this
source-delivery workflow. `check_bundle.py` verifies runtime closure/signatures
and selected notice directories, **not** license compliance, source completeness
or legal permission to release. Original RC1 is preserved as historical evidence;
its missing curl notice is fixed only in newly assembled developer bundles.
