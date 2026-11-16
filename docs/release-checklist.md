# 0.1.0 first-release checklist

Current **developer candidate**: 0.1.0 build 5 also suppresses initial controller
HTTP declarations missed in build 4. Build 4 was user-reported working but PS5
keyboard/controller/microphone icons remained. Build-5 compatibility and any icon
change are unverified; no silent fallback exists. See the
[policy checkpoint](reviews/capture-only-negotiation.md) for artifact/evidence.
All independent/source-delivery/manual gates below remain open.

Earlier offline-audit **developer** build: 0.1.0 build 3 at
`build/package-offline-audit/Free PS5 Capture Card.app`. It adds bounded control
framing (third locked patch), C API v3 typed PIN request IDs, credential ACL
validation and explicit CI provisioning. **27 Bootstrap checks, 92 app tests, 12 native targets and
15 package binary checks pass**; hosted CI/device acceptance is not claimed.
See [remediation evidence](reviews/offline-audit-fixes.md). That build retained
legacy negotiation; the user subsequently approved the enabled build-4 policy.

This is a **release candidate**, not a claim that every release gate has passed.
The supported build currently targets Apple Silicon and macOS 26+.

## Candidate scope

- One native window with persistent capture controls and clear current health.
- Typed states for source connection, media preparation/pause, protected content,
  OBS connection/output start, streaming, passcode, retry, stopping and error.
- Historical output resets/source retries separated from current issues and kept
  behind the remembered metrics disclosure. Counter drift is not labelled A/V sync.
- Strict visible port and pairing-code validation; explicit PIN/Stop progress.
- Consistent account, pairing, address and passcode sheets; cancellable discovery.
- Existing capture-frame icon, local credential-file policy and compressed
  video-plus-audio architecture retained. No preview, playback or capture bypass.

## Developer evidence

The living evidence and exact commands are maintained in
[the execution plan](plans/release-ui-polish.md) and
[validation notes](validation.md). Off-screen screenshots in `build/ui-polish`
use real SwiftUI views with synthetic credentials and engine data only. They are
layout evidence, **not** a substitute for interactive accessibility/device tests.

- [x] Final app/native/status/transition/protected-content regressions pass.
- [x] Focused sanitizer and policy/build checks pass.
- [x] Debug/Release app builds and UI-test compilation pass.
- [x] Light/dark/narrow screens and all setup sheets rendered/reviewed.
- [x] Packaged runtime closure, icon, version and signatures verified for RC1.
- [x] Author source audit completed; curl-notice regression fixed for new bundles.
- [ ] Corresponding-source delivery assembled/tested and all distribution notices
  independently reviewed. Original RC1 omits curl's required notice; do not distribute it.
- [ ] Independent code/license/security review completed and findings resolved.

Current local candidate: `build/package-release-candidate/Free PS5 Capture Card.app`
(0.1.0 build 2), archived as `build/FreePS5CaptureCard-0.1.0-rc1-arm64.zip`.
Developer verification: 88 app tests, nine native targets, 11 policy/build tests,
focused transition/protected-content tests and ASan/TSAN checks. Thirty real-view
test renderings are available; interactive UI automation remains unverified.

The subsequent [author source audit](reviews/release-source-audit.md) covers all
first-party production text, both locked patches and build/packaging entry points.
It is not independent review. A separately assembled developer audit bundle at
`build/package-source-audit/Free PS5 Capture Card.app` (also 0.1.0 build 2) adds the
missing curl notice and updated project notices; it is not RC2. It passed 28
Mach-O checks. Fresh audit checks: 13 Bootstrap tests and 10 CTest targets
(upstream unit plus nine capture targets). Original RC1 and its ZIP are unchanged.
See [source-distribution requirements](source-distribution.md) before distributing
any binary; notices/recipes alone are not complete corresponding source.

Earlier optimized **developer** build (not a renamed RC1/RC2):
`build/package-minimal-portable/Free PS5 Capture Card.app`, still 0.1.0 build 2.
It uses the isolated static minimal-FFmpeg runtime and portability boundaries,
while remaining arm64/macOS 26 only. Packaging passed 15 Mach-O checks with
13 dynamic libraries (previously 28 files / 26 libraries). App file bytes fell
from 47,619,118 to 19,949,863 (~58%); the separately named developer ZIP is
8,542,864 bytes. Original candidate artifacts are preserved. Fresh test/device
evidence is tracked in [the active plan](plans/minimal-ffmpeg-portability.md);
the manual and independent gates below remain open for this build as well.

## Hands-on acceptance still required

- [ ] Current candidate: sign-in/pairing/manual-IP/forget flows, cancellation and
  storage errors. Never collect or publish real OAuth URLs, passcodes or keys.
- [ ] Keyboard focus, VoiceOver, light/dark appearance, window resize, close/reopen
  from Dock/menu, Stop, Quit and passcode submission/rejection on a real session.
- [ ] Current candidate: PS5 → OBS picture and source-mixer audio for H264, H265
  and H265 HDR; game transitions and protected-media entry/exit without crashing.
- [ ] Measured A/V sync at minute 1 and minute 30: residual mismatch ≤40 ms and
  drift ≤20 ms; use a suitable permitted source, **not PS5 YouTube/media apps**.
- [ ] Verify actual HDR bit depth/range/PQ/BT.2020 and final display/recording colour.
- [ ] Investigate reported ~1 s TV-to-OBS latency separately from lip-sync. Existing
  OBS screenshot shows 1 MB network buffering, hardware decoding on and 1 s
  reconnect delay; none alone proves the delay's cause.
- [ ] Longer capture/transition soak, network loss, consumer recovery, sleep/wake,
  CPU/RSS and OBS-drop checks. No promise of continuous capture through sleep.
- [ ] Launch on a clean account/machine without developer paths and verify local
  network permissions and signed-bundle behavior.

Earlier user evidence confirms real pairing, all three synthetic OBS feeds and
one HDR-game transition with **source retries 0 / OBS resets 2**. That evidence
must not be relabelled as calibrated HDR, precise sync or full current-candidate
acceptance. Protected-content crash handling has automated evidence; live
entry/exit confirmation remains separate.

## Distribution authorization and limitations

- Developer bundles are ad-hoc signed, **not notarized** or Developer ID releases.
- No publishing/upload, GitHub release/tag, Applications install, autostart or
  notarization is authorized by this checklist. Request separate authorization.
- If public distribution is approved, choose signing/notarization and packaging
  deliberately; do not silently disable Gatekeeper or alter system settings.
- Preserve LICENSE, third-party notices/embedded notices and a reviewable changelog.
- Clearly disclose unencrypted owner-only credential storage and the unauthenticated
  localhost OBS endpoint. Do not describe either as encryption or app-only access.
