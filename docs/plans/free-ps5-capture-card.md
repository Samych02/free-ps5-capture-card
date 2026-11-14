# Free PS5 Capture Card — Standalone App Implementation Plan

## Execution status

**Storage amendment (2026-11-14):** the user explicitly replaced all Keychain /
no-plaintext requirements below with the owner-only, unencrypted local file
described in [the current architecture](../architecture.md). Start fresh;
do not read, migrate or delete old Keychain entries. OAuth tokens/passcodes remain
transient. Historical Keychain milestones/checklists below are not current policy.

Latest continuation: Tasks 6–10 are implemented through the native live session,
recovery/UI, recursive developer packaging and CI configuration. See
[`finish-live-capture.md`](finish-live-capture.md) for current checkpoint commits
and [`../validation.md`](../validation.md) for fresh evidence. Real PS5/OBS media,
sync/HDR/soak, GUI automation, clean-machine launch and independent review remain
open acceptance gates. Historical task/gate text below remains the design
contract; user instruction deferred hands-on checks until final handoff.

This repository copy is the active execution artifact. The text describing Plan
mode and an empty repository below records the review-time state, not current
restrictions. The user approved starting implementation and explicitly approved
installing the required Homebrew dependencies.

- Session moved to `/Users/sacherif/Projects/free-ps5-capture-card`.
- Pinned upstream SHA verified against GitHub main; reference checkout untouched.
- Homebrew prerequisite installation and isolated Python setup completed.
- Task 0 baseline build passed (263 build steps); CTest passes 1/1 target and
  the upstream unit runner passes 118/118 tests, zero skipped.
- Bootstrap entry-point tests: four failures before implementation, then four
  passes after implementation (`python3 -m unittest discover -s Tests/Bootstrap -v`).
- Shell syntax checks pass; patch applies cleanly to the pinned reference source;
  dependency lock patch SHA-256 verified. No app or live-media tests run yet.
- Linked Homebrew libraries require macOS 26; macOS-15 deployment remains
  unverified and is a distribution blocker, not silently claimed supported.
- Initial scope remains all three codec modes, mandatory audio/video, and a
  configurable port. Pairing and live capture are not implemented.
- Task 1: native C framework, worker/Stop/join ownership, Swift adapter, and a
  minimal native utility shell implemented; Debug and Release builds pass.
  Native tests 3/3 and app tests 7/7 pass after observed red runs. Full core and
  bootstrap regression suites remain green. Start Capture is intentionally
  disabled and engine Start returns unavailable, not fictitious success.
- Baseline pushed to `origin/feat/native-app-shell`. Independent code review and
  manual visual inspection remain pending; no merge to main is requested.
- User subsequently requested all work stay on `main`; main includes the verified
  app-shell commit. Do not create per-milestone branches or block on remote-main
  publication. Task 2's automated prototype now works: compressed video+Opus
  Matroska remuxing, loopback replay, all three required fixtures, payload/hash and
  HDR signaling tests, TCP reconnect/port/cancellation checks. Six integration
   methods pass along with previous suites. The user subsequently confirmed that
   the H264, H265 SDR, and H265 HDR feeds all work in their OBS setup. Basic OBS
   A/V compatibility is accepted for progressing to Task 3 (independent account
   setup). Precise A/V measurements, final HDR color/output correctness, manual
   reconnect behavior, and live-PS5 tests remain explicit validation items; the
   user's smoke-test report is not treated as proof of those properties.
- Task 3 implementation: native PSN account flow, Keychain abstraction/system
  store, account model, masked browser-redirect UI and restore/forget controls
  added. Mocked auth/storage/model tests pass; current app suite totals 25 cases.
  The user subsequently reported completing manual sign-in, accepting the real
  account flow for progression to Task 4. No console pairing is claimed yet;
  precise persistence/access-policy diagnostics remain separate if needed.
  Replay feeds stay stopped.
- The user reported completing manual PSN sign-in, then requested external-browser
  deep-link return (explicitly not an embedded browser). They authorized skipping
  it if not straightforward. A credential-free check rejected the proposed custom
  callback with HTTP 400/invalid-redirect; Apple's HTTPS auth callback requires
  association with the Sony-owned domain. Automatic return is deferred, with no
  change to the working external-browser flow. See docs/account-setup.md. The next
  implementation milestone is local PS5 discovery/pairing, not browser tooling.
- Task 4 implementation: owned native discovery/registration workers, immediate
  credential/snapshot copies, nonblocking cancellation plus joined teardown,
  Swift adapters, local discovery/manual-IP pairing sheet, account/MAC/target
  validation, and atomic versioned account+console Keychain storage are added.
  Existing account-only records migrate on a successful write; failed/cancelled
  replacements preserve prior pairing. Capture remains disabled.
- Task 4 automated gate passes: 4 CTest targets (including 5 new native setup
  cases), 41 XCTest cases, and 4 bootstrap cases; Debug/Release builds pass.
  A test-reproduced shutdown-vs-join race was fixed with tracked async cleanup.
  Real PS5 pairing/relaunch, invalid code/manual-IP behavior, and actual macOS
  permission/Keychain policy are pending Gate C. Do not begin live-session work
  on the assumption that mocked registration establishes console compatibility.
- Gate C follow-up: the user reported **paired securely**, then confirmed the
  PS5 was still paired after reopening. Real registration and user-reported
  Keychain recovery are accepted for progression to encoded media. This does not
  establish whether discovery or manual IP was used, invalid/expired-code or
  denied-store behavior, or a successful live media session.
- Task 5: added the alternate pinned-core encoded-media sink, explicit per-track
  configuration generations, raw source counters, receiver-entry monotonic times,
  audio timing through jitter/FEC, explicit missing-audio markers, video recovery
  flags/reference acceptance, an owning bounded-copy adapter, and a synchronized
  keyframe/stop/detach boundary. Headers carry the raw available codec/color
  information; nominal fps and the effective codec selection are not treated as
  proof of actual cadence, HDR signaling, or source A/V phase.
- Task 5 verification: 14 new real-receiver/adapter/lifetime cases pass; the full
  suite passes 5 CTest targets, 41 XCTest cases, and 4 bootstrap cases. Independent
  fresh fetch, patch/hash/file-byte equivalence and complete native tests pass.
  ASan/UBSan passes the media target 10 times. Debug/Release builds and signatures
  pass. Task 6 (shared timeline, bounded queues, live remux validation) is next;
  live capture remains disabled. Independent review remains a release gate.
- Task 6, first verified batch (not full Task 6 completion): added one common
  monotonic-clock mapping with frozen per-track least-delay estimates, independent
  source-counter unwrapping and rational time; a fixed-slot, shared-budget packet
  queue with owned headers/packets and leased-buffer accounting; bounded pre-roll,
  generation/epoch guards, no-consumer discard, required two-track readiness and
  reset signals; actual NAL-type random-access classification; and live stereo
  OpusHead/TOC-duration helpers. Five timeline, twelve queue and five codec-unit
  cases pass, including the two-hour simulation and concurrent producers.
- Task 6 integration is still pending: validate actual SPS/VUI cadence/reordering
  and HDR/SEI/colour metadata without decoder contexts, connect the borrowed-input
  queue admission to live receiver callbacks, harden and measure mux-owned total
  buffering, and validate live Annex-B/Opus remux equivalence. The queue's 16 MiB
  budget is owned queue/configuration storage (including held leases), not proof
  of a total-process or combined FFmpeg memory bound. A reset epoch is a signal
  the future output owner must use to close TCP/mux state, not a socket close by
  the queue itself. Do not enable capture or mark Task 6 complete yet.


**Goal:** Build a minimal standalone macOS app that connects a PS5 to OBS, delivering equally important game video and game audio together without a playback window, local sound, or intermediate media decoding/encoding.

**Architecture:** A native SwiftUI application owns account setup, pairing, stream settings, and Start/Stop. A small C/C++ engine reuses a pinned, minimally patched `chiaki-lib` for Remote Play, and uses FFmpeg's muxing APIs to put compressed video and Opus audio into live Matroska over localhost TCP. OBS's existing Media Source receives both tracks and decodes both; the app does not use Syphon, a virtual audio device, or a custom OBS plugin. A narrow C framework interface separates the native UI from the engine.

**Constraints:**
- This document is for user review. Do not implement while the session is in Plan mode.
- Implementation destination: `/Users/sacherif/Projects/free-ps5-capture-card`. It is already an initialized, empty Git repository, with no commits at the time of inspection. Do not initialize it again or modify its remote configuration.
- Reference checkout: `/Users/sacherif/Projects/chiaki-ng`, inspected at `43728af2e639fecb8a7af1cd68c29820a40de973`. Do not edit, reset, or require installation of that checkout.
- This plan supersedes `/Users/sacherif/.opencode/plan/chiaki-obs-bridge.md`. No importing Chiaki settings, Qt application, or user-facing CLI is required for the new product.
- Only plan documents under `/Users/sacherif/.opencode/plan` may be written during the current phase. No implementation, dependency downloads, commits, or project/session relocation is performed now.
- Video and audio are both required. Neither is an optional add-on; the stream does not count as working unless both are present and synchronized.
- No controller/input capture, microphone, preview, audio playback, filtering, scaling, HDR tone mapping, or media transcoding in this app.
- Treat SwiftUI, the version-1 scope, and the defaults below as design proposals for the user's review. The user has already approved the separate-app direction, combined compressed transport to OBS, and a configurable port.

**Acceptance Signals:**
- A new user can sign in to PSN, pair a locally reachable PS5, configure a stream, and get game picture and game sound in a single OBS Media Source without running Chiaki.
- The app continues capture with its setup window closed; no playback window, speakers, virtual sound device, or OBS plugin is required.
- The app never opens a media decoder or encoder. Test-only fixture generation and validation may encode/decode synthetic media.
- In a loss-free test, Opus packets and normalized compressed video NAL payloads are preserved through remuxing. Container framing and codec-header representation may change. Existing Chiaki recovery can modify compressed video references under loss; do not promise original encrypted network packets or bit identity during repair.
- A configurable, persisted local port controls the copyable OBS URL; invalid/busy ports produce a clear error and never silently switch to another port.
- Required codec selector: **H264**, **H265 (Default)**, and **H265 HDR**, matching the user's supplied Chiaki settings example. All three modes, including HDR passthrough, belong to version 1; HDR is not deferred or silently removed if validation is difficult.
- Actual OBS compatibility, real PSN login/pairing, real PS5 frame cadence, A/V alignment, and reconnect behavior pass the explicit gates below before release claims.
- Initial live targets: 30-minute capture with residual A/V mismatch <=40 ms after any documented fixed calibration, and <=20 ms change between minute 1 and minute 30. Synthetic tests cover two hours, source-counter wraps, loss, restarts, and stalled readers. These are measurement targets, not verified current capabilities.

This is a living execution plan. Maintain Progress, Surprises & Discoveries, Decision Log, and Outcomes & Retrospective as implementation proceeds. All new-project paths below are relative to `/Users/sacherif/Projects/free-ps5-capture-card` unless explicitly absolute. Planned commands are not evidence of tests already run.

## 1. Product and Version-1 Scope

### What ships

- A macOS `.app` named **Free PS5 Capture Card**; initial supported architecture Apple Silicon (arm64), matching the inspected machine.
- One saved PSN account and one saved paired PS5 in version 1. Discovery may show multiple consoles; select one to pair/capture. Replacing it is an explicit action, not an implicit overwrite.
- Local-network streaming only. Internet access is needed for initial PSN sign-in; an already-paired local session must not require an online token refresh every time.
- PSN sign-in using Sony's website in the system browser, followed by a secure field for pasting the final redirect URL. Do not promise an automatic OAuth callback without proving Sony supports an app-controlled redirect for this flow.
- Local PS5 discovery with manual hostname/IP fallback, standard 8-digit console pairing code, and a separate secure prompt if a 4-digit console login passcode is required.
- Stream options: 720p/1080p, requested 30/60 fps, **H264 / H265 (Default) / H265 HDR**, adjustable bitrate with current/recommended readout, local port, and a small optional advanced A/V offset control.
- Start/Stop, copyable OBS URL, connection/consumer status, sanitized error messages, bounded reconnect behavior, and basic non-secret diagnostics.
- Credentials in Keychain; ordinary display/stream preferences in application preferences. No account passwords handled by the app.
- Local developer build and a bundled development `.app`, followed by signing/notarization only if separately authorized.

### What does not ship in version 1

- PS4, Windows/Linux, Intel/universal distribution, 4K, surround audio, or multiple simultaneous streams/OBS clients. HDR passthrough IS included; app-side HDR rendering/tone mapping is not.
- Remote PSN/WAN playback, NAT hole punching, remote console lists, or wake-from-rest automation. PSN login here obtains the identity needed for local pairing; it is not a promise of Internet streaming.
- A gameplay player, live preview, controller mapping, haptics, microphone, screen/desktop-audio capture, OBS scene controls, recording, or streaming-service integration.
- Syphon, BlackHole, a companion OBS plugin, autostart/login items, automatic updates, analytics, or crash reports containing private data.
- Rewriting Remote Play from scratch or wholesale copying of Chiaki's large GUI/settings layer.

### Proposed defaults

- Resolution 1920x1080; requested frame rate 60; codec **H265 (SDR)**; bitrate 15,000 kbit/s. Switching to 720p offers the library's 10,000 kbit/s recommendation, but does not overwrite a user-custom bitrate without confirmation.
- Codec menu labels: **H264**, **H265 (Default)**, **H265 HDR**. Map exactly to `CHIAKI_CODEC_H264`, `CHIAKI_CODEC_H265`, and `CHIAKI_CODEC_H265_HDR`. Persist the selection; do not treat HDR as an app-side filter or a separate postprocessing checkbox. All modes require validation before release. If a particular PS5/OBS configuration cannot support the chosen mode, show an explicit compatibility error; never silently transcode, tone-map, or change codec/dynamic range.
- Bitrate slider: **2..100 Mbps**, in **1 Mbps** steps, matching the inspected Chiaki PS5 slider. Show the selected Mbps value and the resolution-based recommended Mbps separately and clearly (e.g. `15 Mbps (recommended: 15 Mbps)`). Persist/convert exactly to 2,000..100,000 kbit/s in engine options; validate overflow and format. This requests the PS5's stream bitrate, not OBS's final recording/broadcast bitrate, and is not a guarantee of constant delivered bitrate.
- Port 9295, editable range 1024..65535, saved across launches. Bind IPv4 `127.0.0.1` only; no LAN broadcast of the OBS feed and no remote-bind field.
- Example OBS address: `tcp://127.0.0.1:9295`; the displayed/copied value always reflects the saved effective port.
- A/V offset 0 ms by default, advanced range -1000..1000 ms. Positive delays audio. No audio/video enable toggles: both tracks are mandatory.
- Start is explicit; opening the app or completing login must not automatically create a Remote Play session.
- Settings that affect a running session, including port, are disabled until Stop completes. The app never rebinds unexpectedly during a live capture.

## 2. User Experience

One small settings/status window, with three sections rather than a gameplay dashboard.

### Account & Console

1. Click **Sign in to PSN**. Open Sony's login page in the default browser; credentials and multifactor authentication stay on Sony's site.
2. Paste the final Sony redirect URL into the app and click **Finish sign-in**. Treat its authorization code as a secret: do not auto-read the clipboard, persist the URL, or log it.
3. Discover local PS5 consoles, or enter a hostname/IP if discovery is blocked by the network or permission settings.
4. Select the PS5 and enter the 8-digit code shown under the console's Remote Play device-linking settings. Pair using Chiaki's registration protocol.
5. Save console identity and registration credentials in Keychain. Show the paired console's friendly name and editable local address; bind pairing to console identity/MAC, not just its hostname.
6. Provide **Replace console** and **Forget account and console** actions with confirmation. Forget stops capture before deleting stored identities/keys. Cancelling a replacement preserves the old pairing; a new pairing replaces it only after both protocol success and durable credential storage succeed.

PSN sign-in and local console pairing are distinct steps. For local-only capture, derive/store the account identifier and console keys; discard OAuth tokens after identity retrieval unless a later approved feature needs them. Do not implement unnecessary background token refresh or retain PSN credentials merely to show a green account badge.

### Stream Options

Resolution, frame rate, the three-option codec menu (H264 / H265 (Default) / H265 HDR), bitrate slider/readout, local port, and an advanced audio-offset field. Show requested and actual negotiated settings distinctly where they differ, including SDR/HDR and coded bit depth when known. Validate fields before Start and persist only non-secret preferences normally.

Selecting H265 HDR requests HDR directly from the PS5 and forwards the coded HDR stream to OBS. The app does not decode, display, or tone-map it. Correct HDR handling therefore also requires compatible OBS decoding and appropriate OBS project/output color settings; describe that distinction in a concise help message, not by silently forcing SDR.

### Connection / OBS Output

- **Start Capture**, **Stop**, current status, and **Copy OBS URL**.
- Distinguish **Connecting to PS5**, **Connected — waiting for OBS**, **Preparing video and audio**, **Sending video and audio**, **Reconnecting**, and actionable errors. An idle TCP connection is not proof of working audio/video.
- If only one required track arrives, do not show healthy streaming; wait for bounded initialization and then report which track/configuration is missing.
- Valid silent audio is still a working track. Do not use waveform amplitude or an inactive-looking OBS meter as the sole readiness/liveness signal; distinguish valid Opus silence/DTX from an absent track using packet/configuration/timing evidence.
- Closing the setup window leaves an active stream running. The app remains accessible through the Dock/app menu; reopening restores the settings/status window. No new menu-bar subsystem is necessary for version 1.
- **Quit** stops and joins the session cleanly. Stopping/quitting must not intentionally put the PS5 into rest mode. System sleep interrupts networking; recover after wake rather than claiming capture continues while the Mac sleeps.

## 3. Architecture and Dependency Strategy

### App layer — SwiftUI / Foundation / Security

- Native UI and view model on the main actor.
- Foundation URLSession for PSN HTTP requests and URL parsing.
- System browser launch via AppKit. No embedded browser, WebEngine, Qt, or collected PSN password.
- Keychain abstraction for account identifier and pairing keys; in-memory credential store for automated tests.
- Preferences abstraction for stream options and non-secret console display/address data.
- A narrow Swift adapter converts copied native engine events to state updates. Never publish UI state directly from a Chiaki receive thread.

### Engine layer — C/C++ framework with a C interface

- Discovery and registration adapter; session/lifetime adapter; encoded-media adapter.
- Independent video/audio timeline mapping, bounded owned queues, codec-header parsing, Matroska muxer, and cancellable loopback TCP server.
- Opaque handle types and typed callbacks in `CaptureEngine.h`; no Swift code depends on the layout of `ChiakiSession` or Chiaki internal structs.
- Deep-copy ephemeral Chiaki callback buffers before returning. Configuration and packet storage have explicit limits. No socket writes or file I/O from receive callbacks.
- Serialize session start/stop/keyframe requests under one lifecycle owner. UI operations are asynchronous; Stop must not deadlock the main actor by joining a thread that is waiting on it.

### Reuse rather than fork the entire application

Pin the source dependency to `43728af2e639fecb8a7af1cd68c29820a40de973` from the Chiaki-ng repository, including its required pinned submodules. Use CMake FetchContent into the new project's build directory, and store any necessary patches in this repository.

- Fetch only during explicit bootstrap/configuration, not at runtime. If the pinned revision cannot be fetched from the upstream URL, stop and resolve provenance with the user; do not silently use a different revision or an absolute dependency on the reference checkout.
- Apply a small ordered patch series to the fetched dependency copy, never to `/Users/sacherif/Projects/chiaki-ng`.
- Keep patch application idempotent and fail closed if the expected upstream files/hashes no longer match. A successful patch against the wrong revision is not an acceptable build.
- Record the pinned source, required submodules, patch list, dependency versions, and licensing in `Dependencies.lock.json` and `THIRD_PARTY_NOTICES.md`.
- License the derived app under compatible AGPL terms and preserve Chiaki's existing notices/exception text rather than assuming that linking a separately named library permits closed-source redistribution. Review FFmpeg and other bundled dependency license/build configurations before distributing binaries.
- Compile Chiaki's GUI/CLI, FFmpeg decoder, Pi decoder, Opus playback wrapper, Steam/Steam Deck integration, SDL/controller, Speex, and Setsu features OFF. Use the core transport/crypto/registration components; other core code may remain compiled if upstream's build couples it, but must not be invoked accidentally.
- Link FFmpeg avformat/avcodec/avutil only for public container, codec-parameter, packet, and parser helpers. Linking a codec library is not itself decoding; test that no production decoder/encoder context is opened and no decode/encode send/receive calls are used.
- Adapt the small PSN account-ID flow from `gui/src/psnaccountid.cpp`, `gui/src/psntoken.cpp`, and related URL definitions into a native service. Preserve attribution, but do not inherit Qt settings or unsafe full-URL error reporting.

### Build and packaging proposal

- CMake >=3.27 + Ninja for a C/C++ `CaptureEngine.framework`, native tests, replay test server, and pinned Chiaki dependency.
- Set the new engine's C++ standard per target (C++17) rather than relying on upstream's C++11 global setting. The public framework header is C-only, installed under Headers with a matching Modules/module.modulemap so Swift can `import CaptureEngine` without importing Chiaki headers.
- XcodeGen `project.yml` generates a native app and XCTest/XCUITest targets; generated `.xcodeproj` and build output are ignored, not hand-edited.
- `scripts/build.sh` builds the engine first, generates the Xcode project, then builds the app. `scripts/test.sh` runs native CTest and XCTest. One documented entry point avoids manual library-search-path setup.
- The Swift app embeds/signs the engine framework. The development packaging script bundles non-system transitive dynamic dependencies, fixes install names/rpaths, and signs only after dependency rewriting. No absolute Homebrew or reference-checkout paths may remain in a distributable bundle.
- Target macOS 15+ / arm64 initially. Verify dependency deployment targets and run a macOS-15 CI build before claiming that minimum; do not let upstream's host-dependent forced target silently raise it. If contemporary Homebrew binaries require a newer OS, either build compatible dependencies or ask approval to raise the target.
- No App Sandbox for the initial local developer build; include a truthful local-network permission description and use normal macOS permissions. Do not add microphone, screen recording, or camera entitlements. Distribution signing/notarization and sandbox design are later approval items.

## 4. Protocol and Media Contracts

### PSN authentication

- Open the authorization URL for the compatible Remote Play client flow; percent-encode form parameters properly rather than concatenating unescaped user strings.
- Use a fresh `state` nonce for each attempt and require an exact match in the callback if the Sony flow supports it. Validate that behavior in the real-login gate. If state is rejected or not returned, stop and review the security/compatibility tradeoff rather than silently disabling validation.
- Validate final redirect structurally: HTTPS scheme, exact allowed Sony host and path, no username/password component, exactly one nonempty authorization code, expected state, and no OAuth error. Do not use string-prefix matching that accepts attacker-controlled suffix domains.
- Accept the redirect only for an active user-initiated login attempt, expire/cancel that attempt explicitly, consume its code at most once, and prevent a callback from an older attempt completing a newer one. Do not follow a pasted URL in a browser or send it to an arbitrary host.
- Exchange the code, retrieve the numeric account ID, and encode its exact 8-byte little-endian value required by the registration API. Avoid floating-point JSON/number conversion that can lose precision; test uint64 boundaries and invalid IDs.
- Keep authorization codes and OAuth tokens in memory only for this local identity-retrieval flow; discard after success/cancellation. Do not log request URLs that embed a token, request bodies, response headers/bodies containing secrets, or redirected clipboard contents.
- Timeouts, cancellation, browser retry, expired/reused code, rate limiting, network errors, and provider changes have explicit UI states. No insecure TLS settings or automatic credential scraping.
- Sony's authorization flow is unofficial third-party integration, not a guaranteed public API. Gate real login before UI polish or claims of independent onboarding.

### Registration and storage

- Discovery emits copied console descriptors. Registration uses the account's 8-byte ID, selected PS5 target/address, and an 8-digit pairing code, preserving input validation even when the code begins with zero.
- Treat registration callbacks as borrowing the returned host data: copy all needed identity/key fields before callback return.
- Save the 16-byte registration key, 16-byte RP key, key type, console identity/target, and associated account identity using a versioned Keychain record. Store no Wi-Fi/AP secrets unless actually required; never persist the pairing code.
- Use device-local, non-synchronizing Keychain storage and a stable app-specific service/access policy. Surface Keychain denial/locked-store errors instead of falling back to plaintext. Console display/address metadata may be ordinary preferences; it must reference the matching credential record by identity.
- Optional 4-digit console login passcode is a separate secure, in-memory prompt on demand, not the pairing code. Do not store it in version 1.
- Registration replacement uses a temporary pending record and commits atomically only after successful validation/storage. Cancelling/error must leave the old pairing usable. Do not automatically retry an expired code as a background loop.

### Metadata-rich encoded media

Existing callbacks deliver compressed video and Opus, but do not expose enough information for robust synchronized remuxing. Add a separate alternate `ChiakiEncodedMediaSink` in the pinned dependency patch, preserving existing callback signatures.

- Explicit video configuration and audio configuration callbacks before packet delivery; do not pretend SPS/PPS header bytes are a normal video frame.
- Configuration includes actual codec, resolution, available color metadata/codec headers, nominal frame-rate rational; audio includes codec, channel count, sample rate, samples/frame; both carry a generation ID.
- Packet metadata includes raw 16-bit source frame index, generation, first-observed monotonic receive time, loss/recovery flags, and borrowed data/size. Audio includes sample count and an explicit missing-packet marker. Capture receipt time at receiver entry and preserve it through the audio jitter buffer, rather than stamping callback-delivery time.
- The sink is an alternate consumer, not a tee. Validate conflicts with legacy sinks at registration and again before start. Registration is pre-start only; no runtime setter races.
- Video acceptance must continue existing reference tracking/recovery behavior even when the new sink is the only consumer. Absence of an OBS client means discard-but-accept, not a fake decoder failure.
- Audio callbacks run after releasing receiver locks. Missing audio callbacks are timing holes, not zero-length Opus packets to mux.
- Keep library API changes contained in patches, with regression tests for legacy-only behavior. Do not claim binary ABI compatibility for changed public structs.

### Timing and synchronization

- Video and audio are mapped onto one common session timeline; never independently set their first delivered packet PTS to zero.
- Unwrap video/audio source counters independently using sequence arithmetic; handle wrap, duplicates, reordering, and genuine session reset.
- Derive video progression from frame index and verified coded/negotiated cadence; derive audio progression from source position and exact samples/rate. Missing packets leave time gaps instead of shortening the stream.
- During a bounded initial observation window (at most 250 ms), for each track estimate the least-delay offset as the minimum of first-receipt time minus source-media time. Keep both estimates relative to the same monotonic session clock. Freeze that relative mapping for the PS5 session; an OBS reconnect subtracts one common start epoch, not two new independently chosen origins.
- Arrival-time anchoring is an approximation; it cannot establish exact source A/V phase. A measured fixed audio offset may be necessary. Source counters do not prove common clock alignment.
- Requested maximum fps is not proof of actual cadence. Test 30-fps content under a 60-fps request and inspect codec timing where available. Current Chiaki decoder code itself synthesizes/adapts timestamps, so do not assume an existing ready-made shared presentation clock.
- Use equal PTS/DTS only after verifying a low-delay stream without presentation reordering. Detect true random-access NALs: H.264 IDR, HEVC valid random-access points; handle CRA/RASL explicitly. An I-slice alone is not sufficient.
- No arbitrary timestamp nudging or sample-rate correction to hide drift, and no decoding/encoding as a fallback. If rate/reordering/phase cannot meet acceptance, stop at the timing gate and propose an evidence-backed design revision.
- Apply user audio offset before normalizing both tracks to a nonnegative common epoch. Reject invalid/nonmonotonic output timestamps or implausible jumps through a coordinated stream restart.

### Container and OBS transport

- Live nonseekable Matroska, exactly one required video track and one required Opus audio track; required modes H.264 SDR, HEVC SDR (default), and HEVC HDR. H.264 can be used for the earliest transport smoke, but passing H.264 alone is not version-1 codec acceptance.
- Configure AVC/HEVC codec-private data using public FFmpeg support; allow Annex-B vs length-prefix representation changes without altering canonical compressed payloads.
- Build valid OpusHead from the live receiver's supported parameters. Do not copy synthetic-file encoder lookahead/pre-skip blindly into live PS5 packets. If the wire protocol supplies no pre-skip, use an explicitly tested live-receiver initialization (initial candidate pre-skip 0), and validate startup audio alignment at the real-console gate.
- Initial supported real audio baseline is PS5 stereo/48 kHz Opus. Unexpected unsupported channel/rate configurations produce an explicit error, not silent downmix/resampling or dropped audio.
- Preserve known color range, primaries, transfer function, matrix, coded bit depth, and any signaled mastering-display/content-light metadata without assuming full-range RGB or inventing HDR metadata. Retain HDR-relevant VUI/SEI NAL content through remuxing, and propagate container/codec parameters and applicable side data through public FFmpeg APIs when needed. Verify a real HDR mode's 10-bit/PQ/BT.2020 signaling where supplied; a menu label or 10-bit pixel format alone is not proof of HDR. Unknown or conflicting metadata is reported and investigated, not replaced with guessed HDR tags.
- Preserve audio and synchronization identically in HDR mode. Switching codec/dynamic range requires Stop and a new source-session/container generation. If the requested HDR mode is not actually negotiated, report requested versus actual mode and require an explicit user decision rather than claiming HDR or silently changing selection.
- Start with `live=1`, approximately 50 ms Matroska cluster target, bounded interleave delay, nonseekable AVIO, and explicit low-latency flushing. Validate emitted cluster behavior; flushing an AVIO buffer alone does not necessarily finish a Matroska cluster.
- Cap muxer-owned interleave/cluster/AVIO buffering as well as the app queue, and include those buffers in backpressure tests. A bounded input queue alone does not prove the entire output path is bounded.
- App owns a single-client POSIX TCP listener at `127.0.0.1:<port>`; OBS is the client. Validate/reserve the port before starting the PS5 session so an occupied port does not trigger unnecessary console login.
- A worker owns socket/mux/AVIO lifetime; nonblocking send/poll, TCP_NODELAY, macOS SIGPIPE protection, and a 500 ms stalled-write budget prevent receive-thread stalls. Cancellation must interrupt accept, write, header wait, and keyframe wait.
- Loopback is not authentication or encryption. Other local processes may connect; version 1 assumes a trusted Mac and must not describe the feed as protected from other local apps. Reject a second client while one is active; no unsupported fan-out.

### Buffering and startup

- Hard caps: 256 queued entries, 16 MiB combined owned media/configuration storage, 250 ms media age, and 4 MiB individual packet limit. Apply caps before copying/allocating oversized data.
- Without OBS, retain only current bounded configuration and timing state; discard media rather than saving history.
- Before a first random-access video packet, discard predicted video and keep at most 250 ms rolling audio pre-roll. Startup pruning is intentional, not a reason to repeatedly reset an already-empty consumer.
- Wait for both configurations, a verified random-access frame, and corresponding audio; trim both tracks to one shared start boundary. Request a keyframe on consumer attachment through a lifecycle-safe session method.
- Rate-limit keyframe requests to once per second, retry after 3 seconds waiting, and recycle a consumer after 5 seconds without a usable start. The app must not claim healthy output with a missing track.
- On queue overflow, stale media, configuration-generation change, source-session reset, or irrecoverable discontinuity: close the current TCP stream, clear both tracks consistently, and let OBS reconnect for fresh headers/common epoch. Never concatenate a new unrelated Matroska header onto an existing connection or drop a predicted frame then pretend dependencies are intact.

### OBS setup to validate

- Add **Media Source**, not Syphon or desktop capture.
- Local File OFF; Input `tcp://127.0.0.1:<chosen-port>`; Input Format `matroska`; seekable OFF.
- Restart when active OFF; close when inactive OFF; reconnect delay initially 1 second.
- Begin with minimum available network buffering (candidate 0 MB), then measure/change only if needed. Test actual installed OBS controls; upstream options can differ by release.
- Audio Monitoring OFF to avoid playback through Mac speakers; source audio remains available for OBS recording/broadcast.
- For H265 HDR, validate the actual OBS Media Source decoder, project color format/space, and recording/broadcast configuration. Document a tested 10-bit HDR setup (for PQ content, typically a compatible 10-bit format and Rec.2100 PQ project space), preserving the actual source transfer characteristics. SDR-only delivery may require tone mapping INSIDE OBS; never add that conversion to the connection app or describe SDR output as preserved HDR. Verify source interpretation and output metadata as well as visual appearance.
- OBS performs input decoding and normally re-encodes its final scene output. This plan avoids an additional app-side decode/re-encode, not OBS's normal output encoder.

## 5. Proposed Repository Layout

```text
free-ps5-capture-card/
  README.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  Dependencies.lock.json
  .gitignore
  project.yml
  CMakeLists.txt
  cmake/
    ChiakiDependency.cmake
    ApplyChiakiPatches.cmake
    BundleRuntime.cmake
  patches/chiaki/
    0001-embeddable-build.patch
    0002-encoded-media-sink.patch
  App/
    FreePS5CaptureCardApp.swift
    AppDelegate.swift
    AppModel.swift
    EngineClient.swift
    Models/{Account,PairedConsole,StreamOptions,CaptureState}.swift
    Services/{PSNAuthService,CredentialStore,PreferencesStore}.swift
    Views/{MainView,AccountConsoleView,PairingSheet,StreamOptionsView,OutputStatusView}.swift
    Resources/{Info.plist,Assets.xcassets/}
  Native/
    CMakeLists.txt
    include/CaptureEngine.h
    module.modulemap
    src/
      Engine.cpp
      Discovery.cpp
      Pairing.cpp
      Session.cpp
      EncodedMedia.cpp
      Timeline.cpp
      PacketQueue.cpp
      AnnexB.cpp
      MatroskaMuxer.cpp
      LocalTCPServer.cpp
      Diagnostics.cpp
      (corresponding private headers as needed)
  Tests/
    Native/{CMakeLists.txt,main.cpp,InterfaceTests.cpp,MediaSinkTests.cpp,
            TimelineTests.cpp,QueueTests.cpp,MuxTests.cpp,TCPTests.cpp,
            PairingTests.cpp,SessionTests.cpp,DiagnosticsTests.cpp}
    App/{AuthTests,CredentialStoreTests,PreferencesTests,OptionsTests,
         AppModelTests,EngineClientTests}.swift
    UI/CaptureUITests.swift
    Support/{make_fixtures.py,integration.py,check_bundle.py,replay.cpp}
  scripts/{bootstrap.sh,build-native.sh,build.sh,test.sh,package-dev-app.sh}
  docs/{setup.md,architecture.md,validation.md,upstream-maintenance.md}
  .github/workflows/macos.yml
```

App/Native type boundaries may use subdirectories as necessary, but do not create unused extension points for hypothetical future outputs. No user credentials, PS5 footage, generated fixtures, downloaded dependencies, compiled libraries, build directories, or generated Xcode project are committed.

### Native public interface

`CaptureEngine.h` exposes a versioned C-only interface with opaque engine/discovery/pairing handles, typed errors, size/version fields on option structs, and callbacks for copied state/console/pairing data.

- Create/destroy; start/stop discovery; start/cancel pairing; validate options; start/stop capture; request a statistics snapshot.
- Document callback lifetime and execution context. Input credential buffers are synchronously copied when a start operation accepts them, then caller-owned buffers can be cleared.
- Output pointers are borrowed for the callback only; Swift adapter copies immediately before dispatching. No engine-owned pointer may be held by a Swift view.
- Stop is asynchronous and idempotent. Destroy joins the worker on a non-main thread after completion and guarantees no further callbacks. A UI callback never has to run for engine shutdown to finish.
- Internal keyframe requests and socket/config-generation transitions are not public UI APIs. One serial lifecycle owner guards the underlying Chiaki instance.

## 6. Progress

- [x] Confirm standalone app destination and inspect the empty repository without modifying it.
- [x] Confirm both tracks are equally required, decoding belongs in OBS, and local port is configurable.
- [x] Incorporate the user's settings screenshot: bitrate slider/readout, H264, H265 default, and required H265 HDR passthrough.
- [x] Inspect Chiaki PSN login, account-ID conversion, registration/discovery, encoded callbacks, timing limitations, and build boundaries.
- [x] Write standalone plan replacing the earlier CLI/export-settings approach.
- [ ] User reviews framework/scope/onboarding proposal and approves this plan.
- [ ] User switches out of Plan mode; relocate session to target repository before implementation.
- [ ] Tasks 0–10 below, with recorded test evidence and explicit feasibility gates.
- [ ] Final code review and user acceptance; no publish/push/merge without authorization.

## 7. Surprises & Discoveries

- Target repository contains only `.git`; `git status --short --branch` reported no commits on main. Treat its existing Git configuration as user-owned; no remote changes or worktree from an unborn HEAD.
- Inspection machine is arm64 and `xcode-select -p` points at `/Applications/Xcode.app/Contents/Developer`. Other tool availability and Xcode version remain to be verified before execution.
- Chiaki's macOS PSN dialog explicitly chooses an external-browser flow. Its existing redirect handling uses prefix matching and some errors expose full URLs; adapt the behavior without copying those security weaknesses.
- Account-ID retrieval uses an OAuth token in a request path, so ordinary HTTP URL logging can disclose it. Tests must cover redaction on error as well as success.
- Discovery and registration are independent C APIs in `lib/include/chiaki/{discoveryservice,regist}.h`; native onboarding does not need the Qt GUI.
- Existing encoded callbacks lack frame indexes/first-receipt timing and an explicit header-vs-frame event. Metadata additions are necessary for a reliable container/timeline.
- `lib/include/chiaki/common.h` already defines H264/H265/H265_HDR. `lib/src/ctrl.c:1295-1303` requests distinct PS5 streaming types, and `lib/src/launchspec.c:78-83` requests HEVC plus HDR dynamic range without requiring a local renderer. `gui/src/qml/SettingsDialog.qml:1401-1429` defines the PS5 bitrate slider at 2..100 Mbps in 1 Mbps steps and shows the resolution recommendation alongside the selection. These are reuse points, not proof that the new OBS HDR path works.
- `lib/src/ffmpegdecoder.c:143-199` currently synthesizes/adapts timestamps from arrival timing, reinforcing that the common source presentation clock has not been established.
- The reference checkout's required submodules were uninitialized during prior inspection and common build tools were not on PATH. No build/runtime/OBS/authentication compatibility proof exists yet.

## 8. Decision Log

- Confirmed by user: separate minimal app; reuse Chiaki components; PSN login/pairing and stream options; both video/audio equally important; OBS can decode both; combined live Matroska/localhost TCP; configurable port; bitrate control and H264/H265 (Default)/H265 HDR selector from the screenshot.
- Proposed for review: SwiftUI native UI, C framework engine boundary, system-browser redirect paste for first-version login, LAN-only capture, one saved PS5, arm64/macOS-15 target. H265 SDR is now the default and HDR is required scope, superseding the earlier H.264-default/HDR-excluded proposal.
- No settings export/import from Chiaki and no runtime dependency on its installed app.
- Prefer direct compressed A/V via one Media Source over Syphon because Syphon supplies decoded video only and would need a separate audio route.
- Pin source and record minimal patches in the new repository; leave the current Chiaki checkout untouched.
- Capture is still an active Remote Play session. Real TV/controller/account behavior must be checked; this is not passive HDMI sniffing and cannot promise to bypass PS5 restrictions.
- All execution gates below are stop/go decisions. Failing a gate is not permission to add a decoder, audio encoder, virtual device, custom OBS plugin, different OAuth redirect, or WAN feature without review.

## Chunk 1: Foundation and OBS Feasibility

### Task 0: Bootstrap only the approved new repository

**Files:** create `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `Dependencies.lock.json`, `.gitignore`, root `CMakeLists.txt`, `cmake/{ChiakiDependency,ApplyChiakiPatches}.cmake`, `patches/chiaki/0001-embeddable-build.patch`, `scripts/bootstrap.sh`, `scripts/build-native.sh`.

1. After approval and Build-mode switch, set the session directory to `/Users/sacherif/Projects/free-ps5-capture-card`. Run `git status --short --branch`; expect the inspected empty repository or investigate new user changes. Do not create a worktree without a HEAD or initialize/overwrite Git settings. The explicitly selected empty target is the bootstrap checkout.
2. Verify `xcodebuild -version`, `xcrun swift --version`, `command -v brew cmake ninja xcodegen ffmpeg ffprobe pkg-config python3`, and full upstream SHA provenance. Distinguish missing PATH from missing installation. No automatic Xcode installation, license acceptance, or global shell modifications.
3. Bootstrap script may install user-approved missing dependencies: CMake, Ninja, XcodeGen, pkg-config, FFmpeg, OpenSSL 3, protobuf/protoc, json-c, miniupnpc, libevent. Do not install Qt or SDL. Use a project build virtual environment with a compatible pinned Python protobuf, following the upstream generator constraints (initial candidate >=5,<6 with compatible protoc). Never use global pip or `--break-system-packages`.
4. Pin upstream SHA, required submodules, Python package versions, supported compiler/FFmpeg baseline, and patch hashes. Fetch to `build/native/_deps`; reject an unavailable SHA rather than substituting local main. Disable unused upstream features explicitly; permit core tests/MUnit for regression checking. Patch only embeddability/deployment/build behavior needed to avoid GUI/tools and host-forced target surprises.
5. Future baseline command contract:

```sh
scripts/bootstrap.sh
scripts/build-native.sh --config Debug --tests
ctest --test-dir build/native -C Debug --output-on-failure
```

Expected: the pinned transport/core builds and its existing tests pass without app/GUI/playback dependencies. `build-native.sh` performs a documented CMake+Ninja configure with `CHIAKI_ENABLE_GUI=OFF`, `CHIAKI_ENABLE_CLI=OFF`, `CHIAKI_ENABLE_TESTS=ON`, `CHIAKI_ENABLE_FFMPEG_DECODER=OFF`, `CHIAKI_LIB_ENABLE_OPUS=OFF`, Pi/Steam/SteamDeck/Setsu/Speex OFF, explicit OpenSSL/protoc/Python paths, and the intended macOS target. Record the actual generated commands, dependency versions, and failures; do not suppress baseline errors.
6. Stage only the files created for this task and commit `chore: bootstrap standalone capture app dependencies`. Do not push or set up a remote. Initial main-branch commit is intentional in the explicitly selected new repository; later isolation can use a feature branch once a HEAD exists.

### Task 1: Create the tested native boundary and app shell

**Files:** create `Native/CMakeLists.txt`, `Native/include/CaptureEngine.h`, `Native/module.modulemap`, `Native/src/Engine.cpp`, `Tests/Native/{CMakeLists.txt,main.cpp,InterfaceTests.cpp}`, `project.yml`, `App/{FreePS5CaptureCardApp,AppDelegate,AppModel,EngineClient}.swift`, initial model/service protocol files, `App/Views/MainView.swift`, `App/Resources/Info.plist`, `Tests/App/{AppModelTests,EngineClientTests}.swift`, `scripts/{build.sh,test.sh}`.

1. Add failing native tests for opaque handle creation/destruction, option-struct version rejection, callback lifetime, repeated Stop, and engine callbacks arriving from a worker. Add failing XCTest for actor-safe model transitions, fake engine injection, and no automatic start on app launch. Implement minimal stubs so tests fail for missing behavior, not missing dependencies.
2. Run `scripts/test.sh --native --filter interface` and `scripts/test.sh --app --only AppModelTests,EngineClientTests`; expect relevant red tests. The script prints exact CTest/xcodebuild commands and propagates nonzero exits; a zero-test filter is an error.
3. Implement a minimal C framework and Swift adapter; UI initially displays non-secret fake/idle state. No real PSN network or PS5 connection yet. Main window uses native controls; closing it does not terminate an active fake session, while Quit exercises asynchronous stop.
4. `scripts/build.sh --config Debug` must build native framework first, run `xcodegen generate --spec project.yml`, then `xcodebuild -project FreePS5CaptureCard.xcodeproj -scheme FreePS5CaptureCard -configuration Debug -derivedDataPath build/xcode build`. Expected: app and test targets link without Qt/SDL; framework is embedded with valid runtime search paths.
5. Re-run targeted tests, then `scripts/test.sh --native --app`. Expected: baseline/core/interface/model tests pass; app launches without opening audio/video devices. Add no test-only fake login/credential mode to Release builds.
6. Commit explicit shell/interface/model/test files: `feat: add native app shell and capture engine boundary`.

### Task 2: Prove both compressed tracks reach the actual OBS build

**Files:** create `Native/src/{MatroskaMuxer,AnnexB,LocalTCPServer}.cpp` and private headers; `Tests/Native/{MuxTests,TCPTests}.cpp`; `Tests/Support/{make_fixtures.py,integration.py,replay.cpp}`; modify native/test CMake wiring and `docs/validation.md`.

1. Add failing tests for two-track nonseekable Matroska, valid codec configuration, normalized video payload/Opus packet preservation, and a minimal cancellable loopback writer. Fixture generation may encode synthetic media; production/replay path may not transcode.
2. Run `scripts/test.sh --native --filter mux`; expect red before implementation. Generate 10-second H.264 SDR+Opus, HEVC SDR+Opus, and valid 10-bit HEVC HDR+Opus fixtures with synthetic motion and synchronized flash/impulse markers, no B-frames, and deterministic manifests. HDR fixtures must have known reference values and valid color/signaling expectations, not merely an SDR clip relabeled HDR. Required fixture encoders/bit-depth support must fail clearly when absent; do not silently skip required HEVC/HDR coverage.
3. Implement the minimum replay remux/server. Run `python3 Tests/Support/integration.py --build-dir build/native --case remux-equivalence`; expect one video and one audio track, preserved normalized compressed payloads, and coherent durations. Validation can decode in a separate test consumer.
4. Run `build/native/bin/capture-replay --fixture build/native/fixtures/h264-opus.mkv --loop --port 9295`. Loops advance timestamps continuously; they must not rewind the active consumer's timeline.
5. Use the installed OBS Media Source with the settings in Section 4. Verify moving image, an audio mixer signal, an actual recording with both tracks, correct aspect/range, and no source-specific plugin. Repeat source/OBS reconnects ten times, including OBS started before the replay server. Record exact OBS/FFmpeg/macOS versions and decode/buffering settings; ffprobe success alone is not enough.
6. **Gate A — transport feasibility:** use H.264+Opus for the first smoke, then require HEVC SDR+Opus (the default) and HEVC HDR+Opus to work together and reconnect in actual OBS before proceeding to real onboarding/integration work. HDR acceptance includes correct 10-bit/color metadata interpretation, not just a visible image. On failure, stop and discuss the blocker; do not silently remove required HDR support, encode audio to AAC, tone-map in the app, or introduce a plugin.
7. Run `scripts/test.sh --native --app` and commit explicit mux/replay/tests/report files: `feat: prove synchronized compressed AV delivery to OBS`.

## Chunk 2: Independent Account and Console Setup

### Task 3: Implement secure PSN account identification and persistence

**Files:** create `App/Services/{PSNAuthService,CredentialStore,PreferencesStore}.swift`, `App/Models/Account.swift`, `App/Views/AccountConsoleView.swift`, `Tests/App/{AuthTests,CredentialStoreTests,PreferencesTests}.swift`; extend AppModel and dependency notices.

1. Write failing tests with URLProtocol/mocked HTTP for form encoding, strict redirect parsing, duplicate code/state rejection, nonce validation, token/code expiry, cancellation/stale response, precision-safe uint64 account IDs, little-endian conversion, TLS/network errors, and sanitized error reporting. Add tests for Keychain denial, atomic identity storage, sign-out, and no plaintext fallback. Use an in-memory credential-store fake, not the user's real Keychain, in unattended tests.
2. Run `scripts/test.sh --app --only AuthTests,CredentialStoreTests,PreferencesTests`; expect red for missing behavior.
3. Implement the browser/paste flow and HTTP exchange using native APIs. No embedded login form or stored Sony password. Transient auth material is neither preferences nor a persisted Keychain token; only the derived identity is retained for local pairing. Prevent late callbacks from an abandoned login replacing a newer identity.
4. Add privacy-safe manual tests against the real Sony flow using the user's browser, without printing final redirect URLs or token-bearing HTTP paths. Verify state support and account identity before proceeding. Use harness secret cards for any secret needed during development; do not ask for passwords/PINs/codes in chat.
5. **Gate B — authentication feasibility:** actual sign-in and exact account-ID retrieval succeed without Chiaki installed, with approved redirect validation. If Sony rejects the compatible flow/client/state, stop and revise with the user. Do not promise ASWebAuthenticationSession callbacks to a Sony-owned redirect URI or invent an unregistered callback URL.
6. Re-run targeted and complete app/native suites. Verify no credential material in preferences, diagnostics, screenshots, or build output. Commit explicit auth/store/UI/test files: `feat: add independent PSN account setup`.

### Task 4: Discover and pair a PS5 with recoverable credential storage

**Files:** create `Native/src/{Discovery,Pairing}.cpp`, `Tests/Native/PairingTests.cpp`, `App/Models/PairedConsole.swift`, `App/Views/PairingSheet.swift`; extend native C interface, EngineClient, AccountConsoleView, AppModel, and store tests.

1. Write failing native tests for discovery snapshots, registration cancellation/lifetime, borrowed data copying, unavailable target, invalid PIN, and protocol errors. Write app tests for address fallback, 8-digit vs 4-digit PIN separation, leading zero, Keychain failure, replacement cancellation, stale callbacks, and preserving old pairing until successful replacement.
2. Run `scripts/test.sh --native --filter pairing` and `scripts/test.sh --app --only AppModelTests,EngineClientTests,CredentialStoreTests`; expect red for new pairing paths.
3. Implement discovery and registration through `chiaki-lib` on owned workers. Permit manual address entry without pretending discovery permission is required for every routed local connection. Surface local-network denial separately from no consoles found. Copy successful credentials immediately; no registration code retention or raw library dumps.
4. Pairing identity must be saved durably and match account/MAC/target before UI becomes ready. No concurrent replacement can write keys for one console under another's address. Cancellation stops/joins discovery/registration safely; the UI remains responsive.
5. **Gate C — pairing feasibility:** sign in and pair a real PS5 using only the new app; quit/relaunch and recover the paired identity from Keychain. Test invalid/expired code, manual IP fallback, and denied store access. With saved registration keys, local session setup later must work without ongoing OAuth refresh. Document TV/controller/account restrictions rather than trying to bypass them.
6. Run all app/native tests; commit the explicit pairing/discovery/interface/UI/test changes: `feat: pair local PS5 without Chiaki settings import`.

## Chunk 3: Encoded Media, Timing, and the Live Session

### Task 5: Add metadata-rich encoded delivery to the pinned core

**Files:** create `patches/chiaki/0002-encoded-media-sink.patch`, `Native/src/EncodedMedia.cpp`, `Tests/Native/MediaSinkTests.cpp`; modify dependency lock/patch wiring. The patch changes upstream `lib/include/chiaki/{encodedmedia,session,videoreceiver,audioreceiver}.h`, `lib/src/{session,videoreceiver,audioreceiver}.c`, appropriate CMake registration, and upstream core tests as needed. No canonical checkout edits.

1. Add failing tests for configuration-before-sample ordering, frame indexes through jitter/FEC handling, first-receipt timestamps rather than delayed callback time, missing audio markers, sink conflicts, configuration generations, buffer lifetime, callback acceptance/reference tracking, and unchanged legacy paths.
2. Run `scripts/test.sh --native --filter media-sink`; expect red. Keep tests attached to real receiver delivery paths, not just manually constructed metadata structs.
3. Implement the media-sink contract in Section 4; preserve existing callback signatures and core transport/recovery. Add a session keyframe-request boundary synchronized with stop/fini; test request-vs-shutdown races. Never stop/join a session from a receive callback.
4. Save patch files as the source of truth. Verify a fresh dependency fetch plus patch application reproduces the tested source and patch hashes. Do not edit only the fetched build tree and forget to export the patch.
5. Run targeted tests and `scripts/test.sh --native --app`; require existing Chiaki core tests to remain green. Include a legacy-callback regression even though the new app does not ship the old GUI. Commit explicit patch/adapter/tests/lock changes: `feat: expose timed encoded video and audio from Remote Play`.

### Task 6: Implement the shared timeline, bounded queues, and full remux rules

**Files:** create `Native/src/{Timeline,PacketQueue}.cpp`, `Tests/Native/{TimelineTests,QueueTests}.cpp`; extend AnnexB, MatroskaMuxer, MediaSinkTests, fixture/integration harnesses.

1. Add failing tests for independent 16-bit wraps; two-hour rational/sample time; duplicates/reordering/loss; exact audio holes; shared-session clock origin; reconnect without A/V re-anchoring; audio offset sign; frame-rate assumptions; bad DTS/PTS; and config changes.
2. Queue tests cover copies, concurrent producers, caps before allocation, total storage including headers, no-consumer discard, startup pre-roll trimming, stale generations, bounded missing-track initialization, and no undecodable predicted-frame continuation after overflow.
3. Run `scripts/test.sh --native --filter timeline` and `scripts/test.sh --native --filter queue`; expect red. Implement the explicit Section 4 timing/queue policies, not ad hoc arrival-based timestamps on every callback.
4. Harden Matroska/Opus configuration and true random-access classification. Test HEVC CRA/RASL, all three codec-mode mappings, HEVC Main10/HDR VUI and SEI preservation, signaled mastering/content-light data, header/profile/dynamic-range changes, differing container time bases, Opus startup/pre-skip assumptions, missing audio, and container color metadata propagation. Never open codec contexts to make a header parser work.
5. Run `python3 Tests/Support/integration.py --build-dir build/native --case synthetic-clock` and `--case remux-equivalence`; expect bounded storage, no accumulated rounding drift over two hours, complete source-counter wrap coverage, both tracks, and unchanged canonical payloads in loss-free fixtures. Test consumers may decode to check for broken video or audio.
6. Run all app/native tests; commit explicit timeline/queue/mux/test changes: `feat: preserve AV timing with bounded media buffering`.

### Task 7: Connect the real capture lifecycle end to end

**Files:** create `Native/src/Session.cpp`, `Tests/Native/SessionTests.cpp`; extend Engine/EncodedMedia/LocalTCPServer, StreamOptions models, EngineClient, AppModel, and integration harness.

1. Write failing tests for validate/reserve port before console start, start without paired credentials, partial initialization unwind, mandatory two-track readiness, connected-without-consumer discard, passcode request, session generation reset, no decoder/audio device/controller initialization, and Stop while receiving.
2. Run `scripts/test.sh --native --filter session` and `scripts/test.sh --app --only EngineClientTests,AppModelTests`; expect red.
3. Construct a fully initialized `ChiakiConnectInfo`: PS5 target/keys, explicit local host, selected resolution/fps/codec/bitrate, both A/V enabled, no PSN auto-regist/WAN, no keyboard/DualSense, existing IDR recovery enabled, and `packet_loss_max=0.05` matching the inspected default. Choose/report actual negotiated settings; do not silently lose one track or mutate the user's saved request.
4. Bind the chosen loopback port, initialize the session, install the alternate sink before session start, then start receiving and drive both media tracks through the timeline/queue/mux pipeline. On failure unwind the reserved listener and partial session cleanly. Pass login passcode securely only when prompted; no microphone encoder or renderer exists in the engine.
5. Start the real app with saved pairing and add its copied URL to OBS. Confirm picture AND sound in a recording without screen/audio-device capture for H264, H265 SDR (default), and H265 HDR. Validate actual frame order, PS5 Opus initialization, 30/60 content cadence, color range/transfer/primaries/bit depth, startup offset, and fresh headers on source reconnect. Use genuinely HDR-capable console/content/settings for HDR tests; verify OBS interprets the source correctly and preserves HDR in a tested HDR output configuration. Test an explicit OBS-side SDR conversion separately if offered in documentation, never in the app.
6. **Gate D — real-media feasibility:** a short live test must show both tracks usable, no growing timing error, and a reconstructable no-transcode timeline before reliability/UI polish. If the current metadata cannot represent cadence/reordering or shared timing adequately, stop and propose a revision. Do not falsely declare this solved by assigning each stream its own zero timestamp.
7. Run all app/native suites and record sanitized live evidence; commit explicit session/integration/test changes: `feat: deliver live PS5 video and audio to OBS`.

## Chunk 4: Reliability, Complete Minimal UI, and Delivery

### Task 8: Make disconnection, backpressure, and shutdown reliable

**Files:** create `Native/src/Diagnostics.cpp`, `Tests/Native/DiagnosticsTests.cpp`; extend session/server/engine tests, AppModel, AppDelegate, and integration harness.

1. Write failing tests for partial sends, SIGPIPE, client not reading, second client, repeated connect/disconnect, missing headers/keyframe, overflow, port conflict, invalid auth vs transient loss, suspend/wake, late callbacks after cancellation, and signal/quit during every worker state.
2. Run `scripts/test.sh --native --filter tcp`, `scripts/test.sh --native --filter session`, and `scripts/test.sh --app --only AppModelTests,EngineClientTests`; expect red for new scenarios.
3. Implement nonblocking I/O and 500 ms stalled-write budget, bounded startup/keyframe waits, consistent A/V reset on overflow/config change, and a new Matroska context/header/common epoch for each client. Reject a second active client; do not share packet cursors or silently feed stale content.
4. Session retry backoff: 1, 2, 4, 8, then 15 seconds maximum, with small jitter and reset after stable connection. Retry transient reachability/session loss; stop with an actionable state for invalid keys/passcode, protocol incompatibility, or console already in use. Never overlap old/new session instances.
5. On reconnect, invalidate queued generations, close output, join/finalize the old session, then restart. Keyframe requests must not outlive transport. Stop is idempotent; normal local Quit/Stop targets completion within 2 seconds, with longer network failures not blocking the main actor indefinitely.
6. Emit low-frequency counters for both tracks, losses, queue depth/age, effective configuration, and reconnections. No raw verbose protocol mode, full HTTP URLs, tokens, key dumps, or private account fields. UI should show errors without repeating sensitive inputs.
7. Run `python3 Tests/Support/integration.py --build-dir build/native --case faults`; include 100 reconnects and cancellation in accept/send/keyframe/retry states. Expect no indefinite joins, no stale packets, bounded memory, and no continued callbacks after destruction. Run all tests, then commit `feat: recover capture sessions and stop safely` with explicit files.

### Task 9: Finish the minimal, accessible account/pairing/options/status UI

**Files:** create/complete `App/Views/{MainView,AccountConsoleView,PairingSheet,StreamOptionsView,OutputStatusView}.swift`, `App/Models/{StreamOptions,CaptureState}.swift`, `Tests/App/OptionsTests.swift`, `Tests/UI/CaptureUITests.swift`, `App/Resources/Assets.xcassets`; extend AppModel/PreferencesStore tests.

1. Before UI implementation, use `@frontend-design` and the approved native visual direction; do not create a gameplay dashboard. Add failing tests for persistence/validation; codec labels/order and H265 default; exact H264/H265/H265_HDR engine mapping; bitrate slider 2..100 Mbps in 1 Mbps steps and exact kbit/s conversion; selected/recommended readout; disabled editing while active; copy-URL port correctness; retry/readiness labels; stop-before-forget; and reopening a closed setup window without duplicate sessions.
2. Run `scripts/test.sh --app --only OptionsTests,AppModelTests,PreferencesTests` and `scripts/test.sh --ui`; expect new cases red with deterministic fake engine/auth/store injections. UI tests must never request real credentials or connect to a real console in CI. Test-only modes are compiled out of Release.
3. Implement the three compact sections in Section 2. Keyboard navigation, VoiceOver labels, accessible error text, light/dark appearance, secure entry fields, and a clear running-state indicator are required. Account/pairing secrets are not surfaced in screenshots or a debug panel.
4. Persist port and quality settings without persisting passcodes/auth redirects. Copy only the effective `tcp://127.0.0.1:<port>` URL; explain that changing the port requires updating OBS after stopping. Port conflicts never auto-pick a new value.
5. Show requested vs negotiated quality/dynamic range where useful, with both track readiness emphasized equally. Include the required bitrate slider/readout and three-choice codec selector from the user's example, with H265 as default. Add concise HDR/OBS setup help and explicit errors for incompatible negotiation. Advanced A/V offset remains small/optional, not a sprawling DSP settings page. No audio monitor, preview, input mapping, or recording button is added.
6. Run all native/app/UI tests and manually inspect the app on the target Mac. Commit explicit UI/model/resources/tests: `feat: complete focused PS5 capture setup interface`.

### Task 10: Verify, document, package, and hand off

**Files:** create `.github/workflows/macos.yml`, `scripts/package-dev-app.sh`, `cmake/BundleRuntime.cmake`, `Tests/Support/check_bundle.py`, `docs/{setup,architecture,validation,upstream-maintenance}.md`; complete README, license/dependency notices, and test registration.

1. Add failing checks for no production media decoder/encoder calls, expected embedded framework/dependencies, valid rpaths, correct architecture/deployment targets, secret-free artifacts/preferences, and no runtime dependency on an installed Chiaki/Qt/SDL app. Linkage alone cannot prove no decoding because avcodec is a legitimate mux-helper dependency; inspect/instrument actual call sites and test codec construction is not used.
2. Build/test with `scripts/build.sh --config Debug`, `scripts/test.sh --native --app --ui`, and `python3 Tests/Support/integration.py --build-dir build/native --case all`. Expected: native/core, model/auth/storage, deterministic UI, remux/timing, and fault tests pass. Run native ASan/UBSan with dedicated build output; use TSAN where supported and record exclusions rather than hiding failures. Native sanitizers and the Swift/GUI test host may need separate configurations.
3. CI on macOS-15/arm64 installs pinned-compatible prerequisites and runs synthetic/network-local tests only, no real PSN account/PS5. Validate feature-disabled core regressions as well as the patched alternate sink. Keep local manual gates visible as not CI-proven.
4. On the user's actual PS5/OBS, record 30 continuous minutes per required mode (H264 SDR, H265 SDR, H265 HDR). Measure repeatable visible/audible events near minute 1 and minute 30; residual mismatch <=40 ms after any documented fixed offset, drift <=20 ms. Cover 30-fps and 60-fps content; test 720p/1080p, bitrate selection/persistence/conversion, H265 default, nondefault port, invalid/occupied port, source hide/show, OBS restart, temporary network loss, passcode prompts, app window close/reopen, and quit/relaunch. Validate HDR bit depth, source/output transfer/primaries/range, signaled HDR metadata, and visual reference values; both audio and video must remain correct in HDR mode. Explicitly document PS5/content/display prerequisites discovered during testing rather than promising HDR for every game/setup.
5. Check PS5 controller/account/TV behavior while connected and after Stop. Capture cannot override Sony's policy. With saved registration keys and a reachable console, verify local streaming is not blocked solely because the PSN OAuth session has expired or the Mac lacks Internet access.
6. Target OBS-consumer recovery within 10 seconds while the PS5 session remains live, including OBS's configured retry interval. For PS5 reconnection after network availability, target 20 seconds to account for the capped 15-second retry backoff plus handshake/keyframe time; measure rather than promise recovery under arbitrary network delays. Report observed startup/recovery latency, steady-state CPU/RSS, track rates, queue age/bytes, and OBS dropped frames. Do not invent fixed resource savings or claim capture continues through full system sleep. Require no sustained monotonic memory growth in repeated synthetic reconnects.
7. Use `scripts/package-dev-app.sh --config Release --output build/package` to assemble a self-contained developer bundle. Copy non-system dynamic dependencies, fix rpaths/install names, then ad-hoc sign nested libraries/frameworks and the app in proper order. Verify with `python3 Tests/Support/check_bundle.py --app "build/package/Free PS5 Capture Card.app"`, `codesign --verify --deep --strict "build/package/Free PS5 Capture Card.app"`, and `otool -L` on app/framework/bundled binaries. Expected: no `/Users/sacherif/Projects/chiaki-ng` or `/opt/homebrew` runtime references in the delivered bundle. Check transitive dependencies recursively, not just the main executable. A copied bundle is not signed/notarized distribution or App Store readiness.
8. Test the packaged app with the reference checkout and Homebrew developer library paths unavailable in the launch environment, ideally a clean test account/machine. Verify Keychain/network permission prompts and real onboarding; do not mark standalone packaging complete solely because it runs on the development Mac.
9. Write setup/OBS instructions, security limits, local-only scope, controller caveats, how to forget account/console, and what OBS final encoding still does. Use `@maintaining-user-guides`. Include upstream attribution, full applicable AGPL/exception notices, dependency notices, source/build instructions, and an unaffiliated-with-Sony disclaimer; review license compatibility before redistribution.
10. Run `git diff --check` and all relevant tests, record exact versions/commands/pass counts/manual evidence/known limitations, and commit explicit packaging/CI/docs/tests: `chore: validate and package standalone capture app`. Use `@verifying-before-completion` and request an explicit code review before merge/handoff. Do not push, publish, install to Applications, enable autostart, notarize, or modify existing apps without authorization.

## 9. Verification Command Contract

These scripts/targets do not exist yet; the tasks above introduce them. All commands below run from the new repository during execution, not from the old Chiaki checkout.

```sh
scripts/bootstrap.sh
scripts/build.sh --config Debug
scripts/test.sh --native --app --ui
python3 Tests/Support/integration.py --build-dir build/native --case all
scripts/package-dev-app.sh --config Release --output build/package
python3 Tests/Support/check_bundle.py --app "build/package/Free PS5 Capture Card.app"
codesign --verify --deep --strict "build/package/Free PS5 Capture Card.app"
git diff --check
```

- `build-native.sh` and `test.sh` print exact CMake/CTest/xcodebuild commands and fail on build/test/zero-test errors. Scripts may not convert a failed native test into a skipped app test or success exit.
- Native tests use the pinned MUnit infrastructure already present with upstream core tests; no need to invent a testing framework. The new tests exercise real adapter/receiver paths where possible and test doubles only at external boundaries.
- The native runner has named suites `interface`, `mux`, `tcp`, `pairing`, `media-sink`, `timeline`, `queue`, `session`, and `diagnostics`. `test.sh --native --filter NAME` maps to an exact suite and fails if it executes zero tests. App selectors map to explicit XCTest classes in the `FreePS5CaptureCardTests` target; `--ui` runs `FreePS5CaptureCardUITests`. Use `-destination 'platform=macOS,arch=arm64'` for this version's xcodebuild test invocation.
- Xcode XCTest handles authentication, preferences, credential abstractions, main-actor state, and adapter ownership; XCUITest handles the small user workflow with fakes.
- Integration fixtures are generated under `build/native/fixtures`, not tracked sources; child processes and ports are cleaned up on failure, interruption, or success.
- Missing real-console/OBS/auth evidence is a blocked/manual gate, not a green automated result.

## 10. Feasibility Gates and Release Checklist

**Gate A — OBS transport:** actual installed OBS ingests live Matroska over loopback TCP with both tracks and fresh headers after reconnect for all required modes: H264 SDR, H265 SDR (default), and H265 HDR. HDR includes correct coded bit depth/color signaling and OBS interpretation; it is not an optional future feature.

**Gate B — PSN identity:** real browser-based authorization and exact account-ID extraction work securely without Chiaki or an invented callback registration.

**Gate C — pairing:** new app pairs and persists a real PS5 identity/keys independently, including storage failure/cancellation recovery.

**Gate D — live timeline:** real source timing/frame order/audio configuration supports synchronized A/V without app-side decoding/encoding. A 30-minute soak then measures long-run acceptance.

Do not call the product complete until all applicable gates and these outcomes pass:
- [ ] Both picture and sound arrive through one OBS Media Source.
- [ ] Bitrate slider/readout and all three codec options work and persist; H265 is the default; exact engine units/mode mappings are tested.
- [ ] H265 HDR passes actual PS5-to-OBS tests, preserving HDR signaling/metadata without app-side decoding/tone mapping and with synchronized audio.
- [ ] No app-side decoder/encoder, preview, audio device, controller, microphone, Syphon, or virtual sound device.
- [ ] Nondefault port persists, is validated, and produces the correct copied URL; busy port is a clear failure.
- [ ] Independent PSN login/pairing works and credentials are not plaintext/logged.
- [ ] Closing setup does not stop capture; Quit/Stop finishes without leaked callbacks or unintended console sleep.
- [ ] Packet loss, consumer backpressure, source activation changes, and PS5 reconnect do not create stale replay/unbounded memory or permanent desync.
- [ ] A/V alignment/drift measurements and no-transcoding evidence are attached, not assumed.
- [ ] Intended controller/TV/account setup remains usable within documented PS5 limits.
- [ ] Packaged app has no runtime requirement for Chiaki or developer Homebrew paths and passes bundle/deployment/license checks.
- [ ] User has reviewed any failed targets/remaining limitations and accepted the delivered scope.

## 11. Idempotence, Safety, and Scope Control

- Initial repository bootstrap is only after leaving Plan mode. Investigate any files the user creates in the meantime before writing; do not overwrite a now-populated target as though it were still empty. Before implementation, copy this approved document into `docs/plans/free-ps5-capture-card.md` in the new repository as an explicit plan artifact; keep its progress/evidence current there, leaving this external review copy as a reference.
- Keep the reference Chiaki checkout unchanged. Dependency patches apply only to pinned fetched sources and are stored/reproducible in the new repository.
- Every meaningful task follows red test, minimal implementation, targeted rerun, broader rerun, and an explicit-files commit. Implementation follows `@executing-plans` and `@writing-tests-first`; changes to this plan record discoveries instead of silently broadening scope.
- Do not delegate unless the user or another applicable instruction authorizes it. Current plan/review work is controller-thread only.
- Never commit real pairing/account data, auth redirect URLs, token-bearing requests, console footage, or generated binary fixtures. Private validation artifacts remain local only with user consent.
- A local feed is a trusted-local-machine endpoint, not an authenticated media service. No remote binding in version 1.
- If a feasibility gate fails, report evidence and choices to the user. A different output plugin/codec conversion/WAN auth scheme is a new design decision, not a hidden fallback.

## 12. Outcomes & Retrospective

Planning only. The target repository was inspected and remains unmodified; no app code, dependencies, commits, tests, or binaries have been created. This standalone plan removes the old CLI/settings-export assumption and gives onboarding, mandatory audio, mandatory video, source timing, and user-configurable transport equal explicit ownership.

Document review status: all four chunks reviewed in the controller thread against scope, interface/ownership, dependency ordering, verification, and failure behavior. Corrected sink-before-start ordering, callback lifetime, replay-loop timestamp continuity, silent-audio readiness, muxer-internal buffering, and distinct OBS/PS5 recovery budgets. Reconciled the user's codec/bitrate screenshot across scope, defaults, controls, engine mappings, HDR metadata, all codec gates, and live validation: H265 is default and H265 HDR is required. The result is a gated execution plan ready for user review; full user approval is still pending. No independent review, real PSN authentication, real pairing, OBS compatibility proof, or media-performance claim is implied by this document.
