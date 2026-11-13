# Validation evidence

## Baseline setup

- Architecture: arm64.
- Xcode: 27.2 (27B52); Swift: 6.4.2.
- Homebrew: CMake 4.5.0, Ninja 1.13.2, XcodeGen 2.46.0,
  FFmpeg 9.0.2_1, pkgconf 3.0.10, protobuf@29 29.6_2,
  OpenSSL 3.5.9, json-c 0.19, miniupnpc 2.3.3, libevent 2.1.13_1,
  Python 3.13.18.
- Isolated Python protobuf: 5.29.6 (`build/python`, not global pip).
- FFmpeg advertises libx264, libx265, and libopus encoders, needed for synthetic
  fixture generation. This does **not** prove 10-bit HDR fixture generation or
  live OBS ingestion. FFmpeg's Homebrew package installed SDL transitively;
  the capture core disables SDL/controller/GUI use and must not initialize it.
- Homebrew warns protobuf@29 is deprecated and scheduled for disablement on
  2027-03-31. Generator upgrades must be tested explicitly before replacing it.

## Checks performed

1. `python3 -m unittest discover -s Tests/Bootstrap -v`: four failures before
   the build entry point existed; four passes after implementation.
2. `bash -n scripts/bootstrap.sh scripts/build-native.sh`: pass.
3. Dependency patch SHA-256 matches `Dependencies.lock.json`.
4. `git apply --check` against the pinned reference checkout: pass (read-only;
   patch not applied to that checkout).
5. `scripts/bootstrap.sh`: pass; creates only the ignored isolated Python
   environment and installs its pinned protobuf.
6. `scripts/build-native.sh --config Debug --tests`: pass, 263 build steps.
   The first attempt was cancelled by the user; the explicitly requested restart
   completed. GUI, CLI, FFmpeg decoder, and Opus playback remain OFF in the
   generated CMake cache.
7. `ctest --test-dir build/native --output-on-failure`: pass, 1/1 CTest target.
8. `build/native/_deps/chiaki-build/test/chiaki-unit`: **118/118 tests pass**,
   zero skipped. These are core regression tests, not live PS5/OBS tests.

## Build warnings / compatibility limits

- Several linked Homebrew libraries have a macOS 26 minimum, despite the requested
  15.0 deployment target. This is a current-Mac development build, **not verified
  macOS-15 support**. Older-OS distribution requires compatible dependency builds
  or an explicit user-approved change of minimum OS before release.
- Existing upstream curl uninitialized-pointer, OpenSSL deprecated API, Gestalt,
  and GNU array-extension warnings remain visible; they did not fail this build.
  They have not been fixed or suppressed in the reference checkout.

## Pending checks

- Engine framework, SwiftUI app shell, and runtime dependency checks.
- Synthetic video/audio transport, all three codec modes, and OBS ingestion.
- Real PSN login, console pairing, A/V synchronization, HDR, and reconnect gates.
- macOS-15 compatibility: the requested target is not proof that installed
  Homebrew binaries run on that OS; check bundled deployment metadata later.

No PS5 capture or runnable app has been produced yet. Do not interpret bootstrap
tests or encoder availability as media-path verification.
