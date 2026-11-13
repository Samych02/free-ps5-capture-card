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
pairing, or live-PS5 media capability has been verified. Visual
inspection and independent code review are still pending.

## Synthetic transport implementation (OBS compatibility confirmed by user)

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
- The user tested the live feeds at ports 9295 (H265 SDR), 9296 (H264 SDR), and
  9297 (H265 HDR) and reported that all work. Combined with the observed Media
  source meter, this confirms basic A/V ingestion in the installed OBS setup for
  all three required modes. The transport compatibility checkpoint is accepted
  for progressing to account setup; this is not a calibrated HDR-output or
  precise lip-sync measurement.
- Pending: measured A/V sync and correct final HDR output/color configuration,
  high-bitrate backpressure/soak, live Annex-B delivery, and PS5 clock behavior.
  These are not inferred from FFmpeg ingestion or packet hash preservation.

### User OBS screen-recording observation

The user supplied a 5.84-second macOS screen recording. Extracted frames show
the test pattern moving in the OBS canvas, with its displayed time advancing
approximately one second per second. Mixer activity is visible. This is evidence
that the displayed test video is reaching OBS, not yet end-to-end A/V acceptance.

`ffprobe` reports only one H264 video stream in the submitted screen recording;
there is **no audio track**. Consequently the clip cannot verify the test tone,
the audible result or precise audio/video synchronization. On closer inspection,
the **Media source's own mixer meter is active**, not only microphone/desktop
meters. Together with moving video this supports the basic OBS A/V arrival smoke
check; the lack of an audio track in the screen recording does not invalidate
that visual evidence. A direct OBS recording would be needed for waveform/listening
analysis or precise synchronization measurements, not merely to establish meter
activity. The user subsequently confirmed all three codec feeds work in OBS.
Measured HDR output, exact synchronization, and manual reconnect checks remain
separate validation items. The private
screen recording was not copied into Git.

## Native account setup implementation (manual Gate B pending)

- Added native browser-based sign-in, strict redirect/code/state validation,
  bounded ephemeral HTTP transport with redirects/cookies/cache disabled,
  precision-safe 64-bit account identity conversion, and versioned Keychain
  storage. OAuth tokens and redirect URLs are not persisted.
- Initial red run: the 11 new auth/storage tests executed and failed against
  stubs. Four new account-model tests then failed against an empty model.
  Subsequent implementation and broader edge-case checks pass.
- Full `scripts/test.sh --native --app`: CTest 3/3 targets pass (118 core, 3
  interface cases and 6 transport test methods), XCTest 25/25 pass. Bootstrap
  4/4 pass. Tests cover malformed/duplicate/wrong-state redirects, expiration,
  one-use codes, form escaping, cancellation/stale responses, numeric/string
  uint64 precision, denied/corrupt storage, and restore/forget behavior.
- Unit tests use fake HTTP and Keychain boundaries; no real Sony credentials or
  account data were used, and actual login/Keychain entitlement behavior is NOT
  established. Live console pairing remains unimplemented.
- Final Debug/Release builds and ad-hoc signature verification pass, including
  the scrollable account window. Full handoff test run again passes CTest 3/3
  targets and XCTest 25/25 cases.
- See `docs/account-setup.md` for the real sign-in/Keychain/relaunch gate. If Sony
  omits/rejects state or changes the API, stop and investigate rather than
  disabling validation. Synthetic replay feeds remain stopped as requested.

## Local discovery and pairing implementation — Gate C pending

- Added pinned-core registration/discovery adapters with copied values, owned
  workers and joined teardown. Raw registration logging is disabled. Discovery
  only recognizes explicit permission failures without forwarding raw log text;
  an empty list is not reported as permission denial.
- Added the native pairing sheet, numeric-IP fallback, leading-zero-preserving
  eight-digit code validation, account/MAC/target checks, and one atomic Keychain
  record for account plus optional console. Legacy account records migrate on
  write. Failed/cancelled replacement, stale callbacks and denied saves preserve
  the existing pairing. Forgetting account deletes console keys too.
- Red evidence: `scripts/test.sh --native --filter pairing` initially failed all
  four stubbed cases. Initial Swift pairing/model/store tests failed against
  stubs, then native bridge conversion tests failed against empty conversions.
  Additional red runs reproduced discovery permission suppression, missing key
  payload acceptance, and shutdown returning while a native join was outstanding.
- Final `scripts/test.sh --native --app`: 4/4 CTest targets and 41/41 XCTest cases
  pass. Pairing adds 5 native test cases with multiple lifecycle/error modes and
  16 app cases. Bootstrap 4/4 pass. Final Release build passes.
- Debug/Release ad-hoc signature checks pass. A separate `build/native-asan`
  configuration instruments C/C++ with `-fsanitize=address,undefined
  -fno-omit-frame-pointer`, reuses the pinned fetched source via
  `FETCHCONTENT_SOURCE_DIR_CHIAKI`, and builds `capture-pairing-tests`.
  `ctest --test-dir build/native-asan -R '^capture-pairing$' --repeat
  until-fail:10 --output-on-failure` passes all 10 runs. This covers injected
  setup workers/copies, not real network registration, Swift, or all media code;
  TSAN has not been run. No sanitizer finding was reported.
- User-guide audit before/after reports no broken links in its managed scope,
  but this app has no cookbook-managed guide tree (`docs/skills/README.md` is
  absent). Pairing instructions and their README/account-guide links were checked
  directly rather than adding an unrelated cookbook documentation tree.
- No real console/network discovery, pairing PIN, or real Keychain data was used
  by automated tests. Manual PS5 registration, quit/relaunch persistence, actual
  permission behavior, and invalid/expired-code UX remain Gate C. See
  `docs/pairing.md`. Live media and TV/controller/account behavior are not yet
  tested; Start Capture remains disabled. Independent review is requested in
  `docs/reviews/pairing.md`, not claimed completed.

## Real pairing user report

After the pairing build, the user reported **paired securely**. After the app
was reopened they confirmed **yep still paired**. This accepts real registration
and user-reported Keychain recovery for progressing beyond Task 4, not complete
discovery/manual-IP/failure-path coverage or proof of live audio/video.

## Encoded-media delivery — Task 5

- Added `0002-encoded-media-sink.patch` and its SHA256 to the dependency lock.
  Alternate video/audio configuration and packet callbacks include track
  generations, source frame index, first receiver-entry monotonic time, samples,
  explicit audio holes, and video/audio recovery metadata. Audio preserves entry
  time through jitter; config/frame callbacks execute outside its mutex.
- Alternate-sink conflicts are rejected at installation and before start. Legacy
  callbacks remain available and are covered through the same real receivers.
  Video callbacks still govern reference acceptance. An owning C++ adapter applies
  64 KiB-header/4 MiB-packet caps before copying; default discard accepts frames.
- IDR transport access is guarded across stop/close. The lifecycle owner can
  detach under a request gate, then join/finalize outside receive callbacks.
  Threaded tests issue requests during stop/detach and after session finalization.
- Red evidence: `scripts/test.sh --native --filter media-sink` first failed five
  new delivery/conflict/acceptance tests while the legacy regression passed.
  Later runs failed the inactive-transport IDR expectation and all three empty
  adapter/control stubs. The actual wrap-loss regression then produced -65534
  instead of 2 after a failed FEC frame across the 16-bit wrap; sequence arithmetic
  fixes it. The initial FEC test fixture's padding/parity layout was corrected
  separately, not treated as a production receiver bug. An added HEVC rewrite
  regression initially used a single-reference fixture that cannot encode a
  fallback reference; inspection of the RBSP and upstream rewrite policy led to
  using upstream's nine-reference fixture instead. Both HEVC mode values now
  verify the existing reference rewrite and its metadata flag without changing
  the library's recovery policy.
- Final `scripts/test.sh --native --app` passes 5/5 CTest targets and 41/41
  XCTest cases; `capture-media-sink-tests` reports 14/14 cases. Bootstrap 4/4 pass.
  Debug/Release builds and ad-hoc signature verification pass. Existing unrelated
  dependency warnings/macOS simulator diagnostics remain; no new native adapter
  compiler failure is hidden.
- Independent fetch/configure/build in `build/native-media-clean` passes all 5
  native CTest targets. Checked pinned revision, both locked patch hashes,
  already-applied reverse checks, and byte-for-byte equivalence of all 11 patched
  files against the development dependency. Removing only the generated media
  patch from that disposable tree and reconfiguring reapplied the locked series.
  The reference checkout was never modified.
- ASan/UBSan `capture-media-sink` in `build/native-asan` passes 10 repeated runs
  with no reported finding. This is injected-receiver/lifetime coverage, not live
  networking; TSAN and end-to-end session races remain future verification.
- Source search under `Native/src` finds no media decoder/encoder-open or
  send/receive/decode/encode calls. It is not a full future packaged-binary audit.
  Actual HDR/cadence/phase, shared A/V timeline, combined bounded queues, and
  live Matroska session delivery are NOT yet established. Capture remains
  disabled; no test streamed the user's PS5. See the pending review request in
  `docs/reviews/encoded-media.md`.

## Task 6 partial checkpoint — shared clock and owned storage

- Added 5 Timeline, 12 PacketQueue and 5 codec-unit cases. Initial red runs used
  `scripts/test.sh --native --filter timeline`, `--filter queue`, and
  `--filter codec-units` against empty implementations. Additional red tests
  exposed premature reset on initial headers, capacity-driven pre-roll reset,
  missing configuration epoch, a fresh source-counter reset ignored forever as
  reordering, and stale pre-roll incorrectly producing readiness. All are fixed.
- Timeline uses exact rational/sample progression through repeated source-counter
  wraps, a 250 ms frozen least-delay mapping and one shared media origin. Tests
  retain audio holes, positive/negative fixed offset and cross-track phase on
  reconnect; no accumulated per-frame rounding is used. This is constructed
  source-clock evidence, not measured real-console synchronization.
- Owned queue storage is capped before copying, including fixed metadata,
  configuration buffers and leases outside the queue. Tests fill all logical
  entries with leased buffers, then pass an invalid pointer to prove denial
  occurs before copying. Buffers remain valid after queue destruction and are
  charged until their final reference releases. Logical storage accounting and
  conservative metadata allowances are not a claim of a strict process-RSS cap.
- `python3 Tests/Support/integration.py --build-dir build/native --case
  synthetic-clock` passes its C++ two-hour clock/queue suites. The `all` case now
  includes this check as well as the existing six synthetic transport methods.
  Existing remux tests remain unchanged evidence for the old fixture sender;
  they do not prove the new live queue/mux connection works.
- Full app/native regression run passes 8/8 CTest targets and 41/41 XCTest cases;
  bootstrap 4/4 passes. Final Debug/Release builds and both ad-hoc signature
  checks pass.
- Separate `build/native-asan` uses ASan/UBSan; timeline/queue/codec targets pass
  10 repeats each. Separate `build/native-tsan` uses `-fsanitize=thread
  -fno-omit-frame-pointer`; `TSAN_OPTIONS=halt_on_error=1` with
  `capture-queue-tests --no-fork` and `capture-timeline-tests --no-fork` passes
  without reported races. These are synthetic/injected tests, not live network
  receiver or end-to-end shutdown coverage.
- NAL-header utilities classify true H264/HEVC random access and leading pictures
  without decoding; they do not parse actual SPS/VUI cadence, reorder capacity
  or HDR. OpusHead/TOC-duration helpers establish the stereo/48 kHz, pre-skip-0
  candidate only; live packets/startup alignment remain untested.
- **Task 6 is not complete:** codec metadata validation, borrowed-callback queue
  integration, measured mux/AVIO/cluster memory bounds and live Annex-B/Opus
  remux equivalence remain. The output owner must react to queue reset epochs and
  recreate both track states; the queue itself does not close sockets. Capture
  remains disabled, and no PS5 session was started. Review scope is in
  `docs/reviews/timing-storage.md`.
