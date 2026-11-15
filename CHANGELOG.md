# Changelog

## 0.1.0 release candidate — not published

First-release candidate for Apple Silicon Macs running macOS 26 or later.

### Capture

- Forward compressed PS5 video and audio to one localhost TCP/Matroska OBS source.
- H264, H265 default and genuine H265 HDR; 720p/1080p, 30/60 requested fps,
  2–100 Mbps and a configurable local port.
- Keep Remote Play alive during supported video/HDR updates and media pauses.
  OBS output can still reconnect with fresh headers/timing.
- Honor protected-content restrictions with a waiting state instead of a crash.
  No restriction bypass, hidden SDR fallback, app-side decoding or playback.

### Setup and interface

- PSN browser sign-in, local-network discovery, manual-IP pairing, saved pairing,
  address changes and explicit forget actions.
- One native window with current health and persistent Start/Stop controls;
  account management, OBS connection and stream settings below.
- Separate live status from historical reset/retry diagnostics; remembered,
  left-aligned metrics with clear counter-drift limitations.
- Validate raw port input and exact pairing/passcode digits; improve cancellation,
  discovery progress, stopping/submission feedback and window reopen behavior.
- Original capture-frame icon and native light/dark appearances.

### Storage and distribution

- Credentials use an unencrypted owner-only local file. OAuth tokens and passcodes
  stay transient; historical Keychain records are never read, migrated or deleted.
- Debug/Release native build caches are isolated to avoid configuration thrashing.
- Developer bundle is ad-hoc signed, not notarized. No public release is published.
- Post-RC1 author audit: add missing static curl copyright/permission notice to
  newly assembled developer bundles and require its directory during validation.
  Original RC1 remains unchanged and must not be redistributed without correction.
- Add explicit Chiaki/chiaki-ng credits, retained AGPL licensing information and
  the dependency/corresponding-source release checklist. No license grant changed.
- Replace the broad Homebrew FFmpeg runtime with a pinned private static build:
  H264/HEVC parsing and Matroska output only, no registered codecs/input protocols.
  Keep fixture-input libraries separate and preserve static license/build notices.
- Prepare native networking, exact-arithmetic, export and packaging boundaries
  for later platform ports. Builds remain Apple Silicon/macOS 26 only; no Intel,
  Universal 2, Linux or Windows release or cross-platform UI has been added.

### Known release gates

Measured live A/V sync, calibrated HDR colour, reported TV-to-OBS latency,
long-run reliability, current-candidate manual accessibility/device acceptance,
clean-machine launch, complete corresponding-source delivery and independent
review remain open. See
[the release checklist](docs/release-checklist.md).
