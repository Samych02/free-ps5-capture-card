# Repository work

- User explicitly authorized publishing v0.1.0 as a downloadable developer preview
  despite the outstanding independent/manual gates, with short readable notes.
  This supersedes the earlier draft-only hold, not the unverified gate results.

- Active release preparation: `docs/plans/first-downloadable-release.md`. User
  chose a downloadable build-8 app with complete matching source. Keep current
  behavior fixed; source/review/manual release gates are not implicitly passed.

- Keep `main` as the only local and remote branch; use local checkpoint commits,
  not per-milestone branches.
- Do not modify `/Users/sacherif/Projects/chiaki-ng`; dependency changes belong
  in the locked patches applied to fetched sources.
- Multi-hour work uses a living Execution Plan as described in `PLANS.md`.
  The active continuation is `docs/plans/finish-live-capture.md`; the base design is
  `docs/plans/free-ps5-capture-card.md` (its Keychain policy is superseded below).
- Continue remaining tasks sequentially without repeated go-ahead questions.
  Run automated tests while working. Hands-on PS5/OBS gates are deferred to the
  final handoff by user instruction; do not claim them passed beforehand.
- No app-side media decoding/encoding, playback, preview, controller/microphone
  features, secret logging, or hidden HDR fallback.
- User superseded the earlier negotiation deferral: implement an explicit
  capture-only core policy and enable it in a new developer candidate. Suppress
  controller/microphone features/signaling; preserve media acknowledgements,
  heartbeats and restrictions. Test offline; real-PS5 compatibility remains
  unverified. No silent legacy fallback or app-side input features.
- Mic follow-up: user approved a separate explicit MICCONNECTION connected=false
  experiment, an exception to complete mic-message suppression. Never send a
  positive connect/unmute or mic audio. Keep working handshake/media unchanged;
  schema presence does not establish PS5 acceptance or removal of its mic icon.
- User accepted keeping build 8 as-is despite persistent PS5 controller/mic
  indicators. Stop further icon/negotiation experiments unless requested. This
  accepts a developer checkpoint, not independent/device-calibration release gates.
- Current approved follow-up: `docs/plans/minimal-ffmpeg-portability.md`. Minimize
  FFmpeg and prepare portable engine/build boundaries only. Continue building
  Apple Silicon/macOS 26; do not add Universal/Intel/Windows/Linux builds or GUIs
  or lower the OS minimum as part of this work.
- User explicitly replaced the Keychain requirement with a local unencrypted
  credentials file: owner-only directory (0700), file (0600), bounded validated
  records and atomic updates. OAuth tokens/passcodes stay transient. Start fresh;
  never read, migrate or delete old Keychain records. No other credential copies.
- No publishing, installation to Applications, autostart, or notarization without
  separate authorization. Independent review and actual-device evidence remain
  release gates even when development checkpoints pass.
