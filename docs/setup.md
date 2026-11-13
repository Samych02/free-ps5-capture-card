# Set up PS5 capture in OBS

## Prerequisites

- Current developer build: Apple Silicon and macOS 26+. No installer, autostart,
  notarization or compatibility promise for older macOS versions.
- Enable PS5 Settings → System → Remote Play. Use a reachable local console,
  preferably wired Ethernet. There is no WAN/hole-punching feature.
- [Sign in in your browser](account-setup.md), then [pair the PS5](pairing.md).
  Account and console keys stay in Keychain. No settings export from another app.
  Never send codes, redirect URLs or keys in chat.

## Stream options

- H265 is default. H264 is the SDR alternative. H265 HDR requests genuine
  10-bit PQ/BT.2020 and rejects mismatched signalling rather than retagging SDR.
- Choose 1080p/720p, 60/30 requested fps and 2–100 Mbps (default 15 Mbps).
  The bitrate readout is the request, not a measured link rate. Received encoded
  cadence can differ from the requested fps.
- Port defaults to 9295. Stop before editing it, then update OBS to match.
  An occupied port is an error; the app never silently picks another.
- Start Capture opens no preview or speakers. Received dimensions, cadence and
  both packet counters appear after headers arrive.

## OBS Media Source

1. Add **one Media Source**, not screen capture or an audio-input device.
2. Uncheck **Local File**. Paste **Copy URL** from the app, normally
   `tcp://127.0.0.1:9295`.
3. Set **Input Format** to `matroska`. Use OBS's FFmpeg-backed source.
4. Choose a short source reconnect delay (for example 1 second) if your OBS
   version exposes it. Each reconnection receives fresh Matroska headers.
5. Confirm moving video **and** this source's OBS audio mixer activity. Mixer
   activity establishes audio arrival; a recording is needed for precise timing,
   not merely to prove audio arrived.

Keep the source active while testing. OBS's buffering, decode options, source
activation/restart behaviour and final encoder affect latency/recovery. Only one
consumer is supported; a second is closed. Other local processes/users can read
the unauthenticated loopback endpoint.

### HDR

Use HDR-capable PS5 content/settings and a correctly configured OBS HDR workflow
(10-bit/PQ/BT.2020 where appropriate). Verify source range, transfer, primaries
and final output, not merely a visible picture. Display/content/Remote Play
restrictions remain subject to Sony's behaviour. The app preserves encoded data
and signalled metadata: no tone mapping or hidden SDR fallback. Any SDR conversion
belongs explicitly in OBS and needs separate verification. Live HDR acceptance
is still pending.

## Status, errors and stopping

- **Waiting for OBS and both tracks** does not claim OBS decoded anything.
  **Sending audio and video** means both compressed tracks were written.
- Transient loss retries with bounded backoff. In-use, rejected credentials and
  incompatible media errors need action. Stop before retrying a fatal error.
- A login prompt accepts a masked **4-digit PS5 login passcode**; pairing uses
  the **8-digit Link Device code**. Neither is saved.
- For denied Local Network access, check System Settings → Privacy & Security
  → Local Network. Check the PS5 IP if discovery fails.
- Slow/nonreading clients disconnect instead of accumulating unlimited queues
  or replaying stale frames. Reconnect waits for fresh random-access video.
- Stop acknowledges native cleanup before setup becomes editable. Only then can
  you replace/forget credentials. Forgetting an account also removes its paired
  console keys from this app's Keychain record.
- Closing the window leaves capture running. Reopen from the app/Dock. Quit
  disconnects and joins workers without sleeping the console.

PS5 account/controller/TV restrictions are not bypassed. Continuous capture
through Mac sleep is unsupported. Measure real recovery and controller/TV
behaviour in the [final acceptance checks](validation.md).
