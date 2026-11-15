# Minimize FFmpeg and prepare portable foundations

This is the living Execution Plan under `PLANS.md`. Approved scope: two workstreams
only, minimal FFmpeg and portable foundations. Baseline `c51ea59`, main branch.

## Purpose / Big Picture

Continue building only Apple Silicon macOS 26. Replace the app's broad Homebrew
FFmpeg runtime with a pinned, verified, source-built minimal dependency. Preserve
compressed H264/HEVC/Opus, HDR metadata, timing and recovery. Isolate native OS
assumptions so adding Intel macOS, Windows and Linux later does not require a
capture-engine rewrite. Do not build/support those targets, add a cross-platform
GUI, lower the OS minimum, introduce a service or change credential policy now.

## Progress

- [x] User clarified/approved exact scope; read build/engine/fixture boundaries.
- [x] Lock minimal FFmpeg source/configuration; regression-test verified acquisition,
  cache identity, runtime feature allowlist and explicit CMake selection.
- [x] Integrate minimal dependency and prove runtime allowlist plus six focused
  remux/format/HDR tests; full pipeline/fault suite remains for stable checkpoint.
- [x] Isolate native platform/build/compiler boundaries; three structural
  regressions failed before the changes and now pass. Native rebuild is underway.
- [x] Update packaging/notices/source inventory and portability extension guide.
- [x] Run stable final checks and measure bundle/dependency delta.
- [x] Finish scoped controller-thread quality review; report in
  `docs/reviews/minimal-ffmpeg-portability.md` (not independent review).
- [x] Create scoped local checkpoint on main containing this plan, implementation,
  tests, notices and review/validation evidence (no push/publication).

## Context and Orientation

`Native/CMakeLists.txt` currently imports Homebrew libavformat/libavcodec/libavutil
via pkg-config; it produces a macOS framework unconditionally. Native media is
C++17 behind `Native/include/CaptureEngine.h`. The SwiftUI app and auth/credential
implementations are Mac adapters, not candidates for a speculative GUI rewrite.
`LocalTCPServer.cpp` and `Discovery.cpp` embed POSIX/macOS socket details; timeline
and queue code use compiler-specific `__int128`; the public API uses GCC-style
export visibility. `scripts/build-native.sh` uses separate Debug/Release caches.

Fixtures use external Homebrew ffmpeg/ffprobe for synthetic encode/decode checks,
and C++ helpers use Matroska demux/file input and Annex-B filters. Keep these
test-only needs explicit; no app decoder/encoder/network media protocol may be
silently enabled for tests. Establish a bounded practical profile and verify it
with the actual library registry rather than assuming configure options suffice.

Readiness review resolved the fixture-profile ambiguity before implementation:
build **two static profiles** from the same verified source. Runtime has H264/HEVC
parsers and Matroska muxing only; fixtures additionally allow Matroska input,
file protocol and H264/HEVC Annex-B filters. Compile the shared capture media
sources against the fixture profile only for helper executables; never link that
profile into CaptureEngine. Assert the production library registry separately.
This avoids shipping fixture-only input or linking two FFmpeg configurations
into one process. Review verdict: Approved for execution within the user's
existing approval; no new platform support or UI introduced.

## Plan of Work

1. Pin the already used FFmpeg source release and SHA-256. Build from verified
   archive in an ignored dependency cache, no global install/Homebrew mutation.
   Disable autodetection, programs, device/filter/resampling/scaling libraries,
   codecs, networking and unrelated formats. Keep required parsers and Matroska
   output. Resolve fixture-only input needs explicitly and document the result.
   Cache identity includes source hash, profile, compiler/SDK and platform/CPU/
   deployment target; preserve Debug/Release native caches and old artifacts.
2. Select the resulting FFmpeg prefix explicitly in CMake; fail rather than fall
   back to arbitrary system FFmpeg. Keep external test ffmpeg tools separate.
   Add runtime capability assertions and test source/download/configuration logic
   before implementing it. Re-run format/payload/HDR/transition/protection tests.
3. Move OS networking/address/discovery details behind a small platform boundary
   used by current macOS implementation. Keep shared media/lifecycle algorithms
   platform-neutral; use a documented checked-arithmetic/export abstraction where
   necessary. Make framework packaging conditional; fail clearly on unimplemented
   platforms rather than adding untested Windows/Linux implementations. Keep Mac
   shell/Xcode/signing entrypoints explicitly Mac-only. No new OS builds/CI gates.
4. Include minimal-FFmpeg license/provenance/configuration and available matching
   source/build recipe in package evidence. Re-inventory the actual dependency
   closure, compare to the 26-library audit baseline, and update README/licensing/
   extension documentation without claiming all source obligations are cleared.

## Concrete Steps and Verification

Run narrow Python Bootstrap tests for tooling/profile changes and focused native
targets for platform/arithmetic changes. Use `scripts/build-native.sh --config
Debug --tests`, selected `ctest --test-dir build/native/Debug -R ...`, and selected
`Tests/Support/integration.py --build-dir build/native/Debug --case ...` while
iterating. At the stable checkpoint run the full Bootstrap/native suite once,
direct app units if relevant, and package to a new output directory. Verify
all Mach-O dependencies/signatures, minimal capability registry and source hash,
no system-FFmpeg runtime, and record sizes/counts/timings. Do not repeat expensive
full transport tests between small edits. Test fixtures never contact a PS5.

## Validation and Acceptance

One arm64/macOS26 app with unchanged supported modes and passing compressed-payload,
format/HDR/recovery/protected-content regressions. FFmpeg configuration and actual
registry are constrained; no encoders/decoders or network protocols in app runtime.
Build fails on missing/unverified minimal dependency instead of using Homebrew
FFmpeg. Platform details are isolated, with actionable instructions for future
Intel/Linux/Windows work and no claim those systems build or work today.
Independent review, device evidence and public distribution authorization remain
separate release gates. Preserve unrelated deletion of
`docs/plans/file-credential-storage.md` and existing RC1/audit artifacts.

## Idempotence and Recovery

Only modify tracked source via patches; dependencies live in ignored `build/`.
Validate archive hashes before extraction/execution. Never overwrite a completed
dependency cache with a different profile; use profile identity paths/stamps.
No global installs, existing reference-checkout changes or publishing. Record
partial builds and exact next command here if interrupted. Keep local checkpoints
on main, not per-milestone branches. Do not discard user changes.

## Surprises & Discoveries

- Test C++ helpers currently demux fixtures within the same process as capture
  code, so blindly removing all demuxers/file protocols would break the harness.
  Account for this explicitly rather than reusing full Homebrew libraries.
- Both minimal profiles build successfully. Runtime has no registered codecs,
  demuxers or protocols; only H264/HEVC parsers and the Matroska muxer. The baseline
  registry had 717 codecs, 90 parser codec IDs, 183 muxers, 359 demuxers and 67
  input/output protocol entries. Ten non-transport CTest targets pass with the
  minimal profile; six selected remux/HDR cases pass in 2.66 seconds.
- macOS system Python predates tarfile's extraction-filter API. Complete preflight
  rejects absolute/parent paths, links and devices; extraction is to a new directory.
  Six builder tests failed before implementation and now pass. Two initial cold
  attempts failed on unsupported `filter=` before this compatibility correction;
  their separate ignored cache directories are preserved, not reused.
- Future-platform boundaries: socket implementation now owns opaque descriptors
  under `platform/macos`; address validation/discovery destinations are behind
  `platform/NetworkSetup.h`; exact wide arithmetic has one compiler adapter;
  API exports distinguish Windows/shared/static; framework packaging is Apple-only.
  No new platform backend or build has been added.
- Release packaging completed in 13.90 seconds: 13 non-system dylibs / 15 Mach-O
  files, all closure/arm64/minimum OS/signature checks pass. File-size comparison
  excluding symlink duplicates: original RC1 app 47,619,118 bytes; new app
  19,949,863 bytes (58.11% reduction). ZIP 8,542,864 bytes, CRC check passes,
  SHA-256 `4d9ac1cb3008d3efc884b3372b93f4c7b640c0cd4f251084eabc73dd29095fa4`.
  Original RC1 ZIP hash is unchanged. OpenSSL 3 and 4 remain through non-FFmpeg
  dependencies; no claim that the whole Homebrew graph is source-built/minimized.
- Direct app units: 88/88 pass, including real-view layout rendering, 10.81 seconds
  end-to-end. This is not interactive UI or device acceptance. The standalone
  runtime capability executable reports LGPL version 2.1 or later. Full CTest
  completed: 11/11 targets pass in 191.37 seconds; transport used 176.59 seconds.

## Decision Log

- User explicitly corrected Universal 2 scope: **do not add Intel builds now**.
  Prepare extension points only; retain arm64/macOS26 and native SwiftUI.
- Retain the existing C API/shared C++ pipeline; platform adapters, not a new
  framework/UI rewrite. No extra OS builds requested.
- Preserve existing AGPL project license and upstream terms; a smaller FFmpeg
  configuration does not erase remaining dependency/source-review obligations.

## Outcomes & Retrospective

Implementation, automated verification and scoped author review are complete.
24 Bootstrap checks, 11 CTest targets, 88 app tests and 15 package binary checks
pass. Scoped changes are checkpointed locally on main. No new platform is built
or claimed supported; remaining release gates are not waived.

Artifacts: `build/package-minimal-portable/Free PS5 Capture Card.app` and
`build/FreePS5CaptureCard-minimal-portable-dev-arm64.zip`, still 0.1.0 build 2,
not RC2 or a published release. Logs: `build/minimal-ffmpeg-native.log`,
`build/portable-foundations-native.log`, `build/minimal-portable-package.log`,
`build/minimal-portable-app-tests.log`. Dependency build logs/manifests live in
their identity cache directories, with the verified source archive retained at
`build/ffmpeg-9.0.2.tar.xz`. Independent/device/source-delivery gates stay open.
