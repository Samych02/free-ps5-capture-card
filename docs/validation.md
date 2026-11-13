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

- Remaining app functionality, visual/UI automation and distributable dependency checks.
- Synthetic video/audio transport, all three codec modes, and OBS ingestion.
- Real PSN login, console pairing, A/V synchronization, HDR, and reconnect gates.
- macOS-15 compatibility: the requested target is not proof that installed
  Homebrew binaries run on that OS; check bundled deployment metadata later.

## Native interface and app shell milestone

- The baseline was pushed to `origin/feat/native-app-shell`; no push to main.
- Native contract red run: 0/3 cases passed with deliberately unimplemented
  stubs. Then real worker/lifecycle/validation implementation: 3/3 pass.
- App contract red run: five tests failed for missing model/adapter behavior;
  a second red run exposed two missing shutdown guarantees. Final result:
  7/7 app tests pass, including real framework callbacks on the main actor,
  no automatic capture, unavailable capture reporting, and joined shutdown.
- Full `scripts/test.sh --native --app`: pass, CTest 2/2 targets (118 upstream
  core cases + 3 interface cases) and XCTest 7/7 cases.
- `scripts/build.sh --config Release`: pass. Debug build also passes as part of
  the full test command. XCTest launches the app as its test host.
- `python3 -m unittest discover -s Tests/Bootstrap -v`: 4/4 pass.
- `codesign --verify --deep --strict` on both final Debug and Release apps: pass.
  Ad-hoc development signing is not notarized distribution approval.
- `otool -L` on the engine shows no Qt/SDL/rendering library dependencies; local
  Homebrew runtime dependencies remain and are not yet bundled.
- macOS default Bash exposed an empty-array expansion issue in the unfiltered
  app test command. Reproduced failure, fixed to use a nonempty argument array,
  and verified the full unfiltered command passes.
- Xcode reports an outdated **iOS** simulator service and missing optional
  AppIntents metadata; the selected destination is native macOS and tests pass.
  No simulator upgrade/system service change was performed.

The app is a runnable development shell, NOT a functional capture app. No PSN,
pairing, actual OBS ingestion, or live-PS5 media capability has been verified. Visual
inspection and independent code review are still pending.

## Synthetic transport implementation (Gate A manual check pending)

- Working branch is now `main`, as explicitly requested by the user. No extra
  milestone branch or PR is required to continue development.
- Initial red integration run failed for the missing fixture generator/sender.
  TCP tests subsequently failed for unsupported server options. Additional red
  tests caught a queued second consumer and overwriting of existing debug output.
- HDR validation caught missing transfer/primaries in the encoded fixture VUI.
  Bitstream tracing showed unspecified values; explicit x265 colorprim/transfer
  parameters corrected the stream. Frame-level probing verifies PQ/BT.2020,
  mastering metadata, MaxCLL, and actual decoded patch codes. Stream-only probing
  can omit fields resolved after the first decoded frame.
- `python3 Tests/Support/integration.py --build-dir build/native --case all`:
  six test methods pass, exercising H264 SDR, H265 SDR, and H265 HDR subcases.
  Compressed packet hashes match on both remuxed tracks for all modes; validation
  decoding is test-only. Production mux/server/replay code opens no codec context.
- TCP receives both tracks in a separate FFmpeg process. Tests cover ten fresh
  header reconnects per mode, second-client rejection, port errors, read-only
  fixture preservation, and cancellation within two seconds. An eleven-second
  H265 capture crosses the ten-second fixture loop without timestamp rewind.
- Full `scripts/test.sh --native --app`: CTest 3/3 targets pass (118 core, 3
  interface cases, 6 integration test methods), and XCTest 7/7 pass. Bootstrap
  tests remain 4/4 green. Fixtures are small 320x180/30 fps synthetic samples.
- Pending: actual OBS tests in all three modes, observed A/V sync and HDR output,
  high-bitrate backpressure/soak, live Annex-B delivery, and PS5 clock behavior.
  These are not inferred from FFmpeg ingestion or packet hash preservation.
