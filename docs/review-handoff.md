# Independent review requested

## Scope

Review live capture changes from `6a53f90` through the final continuation commits
on `main`, including `99095c1` and the recovery/UI/packaging work. No remote push,
PR, merge or release has been performed. This document requests review; it does
not represent an independent approval.

## Requirements to verify

- Compressed video and stereo/48 kHz Opus are equally mandatory, in one live
  Matroska/TCP stream. No app codec opening, decode/encode, playback, preview,
  controller/microphone devices, tone mapping or hidden HDR fallback.
- Credentials stay in Keychain or bounded session memory; PINs/OAuth redirects
  are transient. Raw core logs/remote errors never reach diagnostics.
- Reserve the configured loopback port before console contact. No automatic port
  switching, concurrent old/new session owners, callback-after-free or UI joins.
- Verify teardown order, IDR gate, cancellation, partial init/start failures,
  stale generation/consumer epochs, shared-clock calibration and no-reorder proof.
  Check once-only global core initialization before session setup, typed explicit
  registration rejection mapping, and credential-memory clearing on Stop.
- Audit every allocation owner: startup copy, parser, leased queue, packet, core
  frame, AVIO and Matroska cluster. Queue allowance is not a process-RSS promise.
- Test whether actual PS5 SPS/VUI/SEI and Opus metadata satisfy admission. Synthetic
  fixtures cannot establish live source phase/drift or exact HDR output.
- Check masked PIN retry/cancel behaviour, setup/options disabled through Stop
  acknowledgement, non-secret persistence, copyable URL and shared window state.
  Loss/hole counters and oldest queued age are diagnostics, not a claim of
  measured end-to-end timing or process memory bounds.
- Inspect recursive bundling, rpaths, architectures, minimum OS, ad-hoc signing,
  license copies and actual FFmpeg/OpenSSL compatibility/source obligations.

## Evidence and limitations

See [validation](validation.md) for command logs, automated results and explicit
pending gates. Native ASan/UBSan passed; UI automation could not initialize on
the development desktop. Basic real pairing and synthetic OBS ingestion are
user-confirmed; live PS5/OBS timing/HDR/reliability are deliberately deferred.
Clean-machine launch and legal/security approval remain outstanding.

Please return findings by severity with file/line references and a reproducer.
Blocking findings must be fixed and affected tests rerun before release.
