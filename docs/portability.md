# Portable foundations, one supported build

**Today: Apple Silicon, macOS 26, SwiftUI.** There is no Intel, Universal 2,
Windows or Linux build/support claim. The purpose of these boundaries is to add
those targets later without rewriting capture/timing/recovery algorithms.

## Boundaries

| Layer | Location | Responsibility |
| --- | --- | --- |
| Frontend/platform services | `App/` | Native window, browser sign-in, credentials, preferences and lifecycle |
| Stable native boundary | `Native/include/CaptureEngine.h` | Versioned C value records, asynchronous callbacks, explicit stop/destroy ownership |
| Shared engine | `Native/src/` | Session lifecycle, parsing, bounded queues, timing, HDR policy, two-track mux/recovery |
| Platform networking | `Native/src/platform/macos/`, `platform/NetworkSetup.h` | Socket lifecycle/options, IP validation, interface/broadcast enumeration |
| Compiler arithmetic | `Native/src/platform/WideInteger.h` | Exact wide intermediates for checked media arithmetic |
| Build adapters | `cmake/CapturePlatform.cmake`, `Native/CMakeLists.txt`, `scripts/` | Backend selection, Apple framework/module packaging, toolchain and app signing |

`LocalTCPServer` has opaque implementation storage: file descriptors/socket-handle
types do not leak into its shared header. Discovery destination storage remains
owned until the upstream service finishes. The shared pipeline sleeps through
standard C++ rather than using POSIX poll as a timer.

The C export macro distinguishes Windows DLL import/export and static consumers;
it does **not** mean Windows has been compiled. Exact wide arithmetic currently
uses Clang/GCC's signed 128-bit type behind one adapter. Unsupported compilers
fail explicitly; do not replace overflow-safe integer operations with doubles.

## Minimal FFmpeg

`Dependencies.lock.json` pins the FFmpeg archive URL/version/SHA-256.
`scripts/build-ffmpeg.py` verifies it before extraction and source execution.
Private build caches are under `build/dependencies/ffmpeg/<identity>/install`;
identity includes recipe/profile, source hash, target, compiler and SDK. Debug
and Release native trees reuse these optimized dependency archives.

- **Runtime:** static libavformat/libavcodec/libavutil; H264/HEVC parsers, Matroska
  output and upstream-selected supporting bitstream filters. No registered
  encoders/decoders, demuxers, I/O protocols or FFmpeg network support. The app's
  own bounded localhost transport is independent of FFmpeg networking.
- **Fixtures:** a separate static profile additionally supports Matroska input,
  file protocol and H264/HEVC Annex-B conversion. Only test helper executables
  link it. Shared production sources are compiled for those helpers separately;
  no process links both FFmpeg configurations. Fixture libraries are not packaged.
- Homebrew `ffmpeg`/`ffprobe` remain **external test tools** for generating/decoding
  synthetic evidence, not application runtime libraries.

`cmake/FFmpegDependency.cmake` requires the explicit verified prefix, checks the
source/profile manifest, and never falls back to system FFmpeg. The
`capture-ffmpeg-policy` test enumerates the actual runtime registries. Source-built
FFmpeg notices/configuration/recipe are preserved even though static libraries
do not appear in the dynamic-library manifest. Homebrew remains in use for other
Chiaki dependencies; the entire dependency graph is not yet source-built/pinned.

## Adding a target later

1. **Intel macOS:** add an explicit x86_64 toolchain/cache adapter to the dependency
   builder, source-build all required native libraries for it, parameterize current
   arm64 Xcode/build/package validation, and test on a supported Intel Mac. Merely
   changing `ARCHS`, reusing arm64 Homebrew libraries or running Rosetta is not
   sufficient. Universal 2 merging/signing is a separate later packaging choice.
2. **Linux:** implement/select the network backend (much of the Mac implementation
   is POSIX but `sin_len`, SIGPIPE options and interface behavior need adaptation).
   Add Linux dependency/toolchain and shared-library packaging adapters. The
   current CMake backend selector deliberately rejects it instead of pretending
   it works. Test headless core first; choose a frontend separately.
3. **Windows:** implement Winsock initialization, handle widths, errors, polling,
   nonblocking I/O, shutdown behavior and adapter enumeration behind the same
   boundaries. Select a compiler with proven exact wide arithmetic or implement
   the arithmetic adapter. Add native dependency/runtime packaging adapters.
   A frontend must port browser OAuth/state/cancellation and credential storage;
   Unix `0600` is not a substitute for owner-only Windows ACLs/atomic updates.

For any target, audit the pinned Chiaki/submodule build options and platform
support, preserve the C API's borrowed-data/worker-join contracts and existing
secret handling, run queue/counter/overflow/media/recovery tests, then obtain real
PS5/OBS and clean-machine evidence. Keep compression untouched and honor content
restrictions; do not introduce decoder/encoder or hidden HDR fallback workarounds.

Do not move Mac auth/storage/UI into shared code merely to fill an abstraction:
their existing service protocols are the replacement boundary. A future port
must implement equivalent validation/protection, not silently weaken it.
Independent license/security review and corresponding-source obligations apply
to every new binary target. See [source distribution](source-distribution.md).
