# Minimal FFmpeg and portability — scoped author review

## Verdict

**Ready for a developer checkpoint, not public release.** Scope follows the user's
explicit correction: optimize FFmpeg and prepare extension points, continue
building only Apple Silicon/macOS 26. No Intel/Universal 2/Linux/Windows build,
cross-platform GUI or lowered OS minimum is included. No subagents/independent
review were used; this is controller-thread author review of the changed scope.

## Reviewed boundaries

- `scripts/build-ffmpeg.py`, `Dependencies.lock.json`: verified archive acquisition,
  regular-file/directory-only extraction, immutable profile/toolchain cache identity,
  per-profile build lock, private install prefix, no system-runtime fallback.
- `cmake/FFmpegDependency.cmake`, native/test CMake files: explicit source/profile
  validation, separate static runtime/fixture dependencies, same capture sources
  in test helpers, no helper loading two FFmpeg configurations or bringing fixture
  input into CaptureEngine. Framework/module packaging is now Apple-conditional.
- `platform/macos/LocalTCPServer.cpp`, `platform/NetworkSetup.h` and its Mac backend:
  socket handles hidden behind implementation storage, existing cancellation/write
  bounds preserved, discovery destinations owned through service cleanup, literal
  address validation unchanged. No alternative backend implemented or claimed.
- `platform/WideInteger.h`, `Timeline.cpp`, `PacketQueue.cpp`, public header:
  exact arithmetic semantics retained behind a compiler adapter; unsupported
  arithmetic fails explicitly; DLL/static export boundary avoids a GCC-only API.
- `Pipeline.cpp`: standard C++ five-millisecond sleep replaces POSIX timer polling,
  without changing transport waits, media timing or protected-content policy.
- Packaging helpers/validator, notice/source docs and portability guide: preserve
  static dependency provenance, reject dynamic FFmpeg/codec dylibs, distinguish
  historical GPL Homebrew candidates from current minimal LGPL FFmpeg. The app
  itself remains AGPL. No claim of complete corresponding-source delivery.

## Persona synthesis and findings

Read-only controller passes: senior-engineer for boundaries/lifetime/build coupling;
security-engineer for verified source execution/extraction and unchanged trust
limits; quality-engineer for red/green tests and actual library capabilities;
content-engineer for user scope/license/platform-support accuracy.

No unresolved defect requiring another scoped code change was established. Two
execution issues were corrected with evidence: macOS system Python does not
support tarfile extraction filters (use full strict entry preflight instead), and
macOS `/var` versus `/private/var` resolution needed normalization in the build
entrypoint fixture assertion. No production defect was hidden by weakening the
test. Cold failed-build directories were retained under their old recipe identities.

Residual release blockers are unchanged: independent code/security/license review,
complete corresponding-source delivery and actual-device/clean-machine acceptance.
Prepared interfaces are not evidence of additional operating-system support.

## Verification

- Red/green Bootstrap coverage: verified source checksum/extraction, target/profile/
  compiler cache identity, runtime/fixture allowlists, explicit CMake prefix,
  static notices/provenance, shared-source platform boundaries and API exports.
  **24 tests pass.** Original feature policy executable fails against Homebrew's
  broad library registries and passes against the new runtime archives.
- Actual runtime registry: **0 codecs, 2 parsers, 1 muxer, 0 demuxers, 0 protocols**.
  Library reports **LGPL version 2.1 or later**. No decoder/encoder was opened.
- Full CTest: **11/11 targets pass**, including upstream unit, core/setup/media/
  timeline/queue/session/diagnostics, all transport/format/HDR/pause/restriction
  integration and runtime capability policy. 191.37 s, including 176.59 s transport.
- App tests: **88/88 pass**, 10.81 s end-to-end using the explicit direct unit
  runner; includes real-view off-screen layouts, not interactive accessibility.
- Fresh developer package: **15 Mach-O checks pass**, arm64, macOS minimum,
  recursive closure and strict signatures. **13 dylibs**, down from 26.
- Byte comparison excluding symlink duplicates: original RC1 app **47,619,118**;
  optimized developer app **19,949,863** (58.11% reduction). New ZIP **8,542,864**
  bytes, CRC passes. Original RC1 ZIP SHA-256 is unchanged. Both OpenSSL major
  runtime closures remain through non-FFmpeg dependencies.
- Package provenance matches current source pin and exact builder bytes, runtime
  profile only. No fixture static archives/test plug-ins are present. Markdown
  links and shell syntax pass. Generic guide audit has only the unrelated missing
  cookbook `docs/skills/README.md`, not a broken app guide.

## Cross-Persona Tensions

Small production dependency versus a useful test harness: resolve with separate
minimal static profiles, not by enabling fixture input in the app or abandoning
compressed-equivalence/HDR tests. Future portability versus speculative scope:
isolate current implementations and fail clearly on unsupported targets; do not
pretend that export macros alone constitute a Windows port.

## Missing Evidence

Real PS5/OBS/current-build sync/HDR/transition/latency/soak and clean-machine launch;
interactive UI/accessibility; independent review; complete matching source delivery.
No new sanitizer claim is made for this checkpoint. See the
[execution plan](../plans/minimal-ffmpeg-portability.md),
[portability guide](../portability.md) and [release checklist](../release-checklist.md).
