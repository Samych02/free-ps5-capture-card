# Repository work

- Work on `main`; use local checkpoint commits, not per-milestone branches.
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
- User explicitly replaced the Keychain requirement with a local unencrypted
  credentials file: owner-only directory (0700), file (0600), bounded validated
  records and atomic updates. OAuth tokens/passcodes stay transient. Start fresh;
  never read, migrate or delete old Keychain records. No other credential copies.
- No publishing, installation to Applications, autostart, or notarization without
  separate authorization. Independent review and actual-device evidence remain
  release gates even when development checkpoints pass.
