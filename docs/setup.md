# Set up PS5 capture in OBS

## Prerequisites

- Current developer build: Apple Silicon and macOS 26+. No installer, autostart,
  notarization or compatibility promise for older macOS versions.
- Enable PS5 Settings → System → Remote Play. Use a reachable local console,
  preferably wired Ethernet. There is no WAN/hole-punching feature.
- [Sign in in your browser](account-setup.md), then [pair the PS5](pairing.md).
  Account and console keys use an unencrypted, owner-only local file, not Keychain.
  Historical Keychain credentials are not imported. This UI update does not reset
  existing file-based credentials.
  No settings export from another app.
  Never send codes, redirect URLs or keys in chat.

## Stream settings

- H265 is default. H264 is the SDR alternative. H265 HDR requests genuine
  10-bit PQ/BT.2020 and rejects mismatched signalling rather than retagging SDR.
- Choose 1080p/720p, 60/30 requested fps and 2–100 Mbps (default 15 Mbps).
  The bitrate readout is the request, not a measured link rate. Received encoded
  cadence can differ from the requested fps.
- Port defaults to 9295. Stop before editing it, then update OBS to match.
  An occupied port is an error; the app never silently picks another.
- Start Capture opens no preview or speakers. Received dimensions, cadence and
  both packet counters appear after headers arrive.
- If the same PS5 gets a new IP, stop capture, expand **Manage account and pairing**
  and choose **Change IP…**. Enter
  the new address and save; existing keys are retained, with no re-pairing or
  new sign-in. Connectivity is checked when capture next starts.

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

- The status panel describes **current health**, not a previous reset:
  **Receiving PS5 media…** waits for both tracks; **Waiting for OBS** means source
  media is ready; **Starting OBS output…** waits for a fresh frame and aligned
  audio; **Streaming to OBS** means both compressed tracks were written. None
  claims OBS successfully decoded or displayed them.
- Expand **Show metrics** in **OBS connection** for received format, counters,
  queue age and **Recent events**. The last OBS reset/source retry is history,
  not a current fatal error. Stop clears it. The output reset count belongs to
  the current PS5 source session. **Source retry attempt** is the backoff stage,
  not a lifetime disconnect count, and resets after a stable session.
- **Counter drift** compares nominal source-counter duration with arrival time;
  **Observed counters** use unwrapped source counters, not displayed FPS. Neither
  is measured A/V sync or total TV-to-OBS latency. Long media-gap recovery restarts
  clock spans. For repeated drops, include current status, Recent events and
  metrics, plus selected mode/bitrate—never credentials or media payloads.
  A reset's **audio-start** value is a startup timestamp comparison, not lip-sync.
- Supported game/HDR metadata changes now keep the PS5 session connected.
  Identical configuration updates and valid HDR metadata updates should also keep
  the existing OBS connection. Actual video parameter-set changes may briefly
  show **Starting OBS output…** while only the Matroska output is refreshed;
  **Source retry attempt** should not increase for these supported changes. Audio capture and
  its sample clock continue, though an OBS output reset temporarily interrupts
  delivery of both tracks on that single source.
  Video counter diagnostics begin a new segment after video reconfiguration;
  this does not reset the source audio clock. A genuine unsupported format or
  HDR-to-SDR switch while HDR is selected still reports an error, not a hidden
  fallback. Keep the requested codec mode unchanged for the transition test.
- A game transition can temporarily stop one or both media tracks. After a long
  pause the app waits for fresh audio and video without restarting Remote Play;
  OBS output can reconnect with a fresh timing epoch. Clock diagnostics restart
  after this real-gap recovery, but **Source retry attempt** should not increase from
  media silence alone. If a genuine source reconnect does happen, its most recent
  initiating failure remains in **Recent events** after recovery. Stop clears it.
- Some PS5 media apps/content cannot be streamed through Remote Play. The app
  displays **Protected content** and waits; it does not capture
  restricted media or bypass the console restriction. Return to the PS5 home
  screen or a permitted game to resume when the console allows streaming again.
  Stop remains available while blocked.
- Transient loss retries with bounded backoff. A fresh connection reporting
  Remote Play in use, rejected credentials and incompatible media need action.
  After an established session is automatically restarted, the app allows busy
  replies for a short 15-second release grace period with retry waits capped at
  2 seconds. Failed/busy attempts never extend that window; Stop cancels it.
  This helps when the console has not yet released the previous session during
  a game/video transition. It does not take over another client or change HDR to
  SDR. A brief OBS reconnect can still occur. Stop before retrying a fatal error.
- A login prompt accepts a masked **4-digit PS5 login passcode**; pairing uses
  the **8-digit Link Device code**. Neither is saved.
- For denied Local Network access, check System Settings → Privacy & Security
  → Local Network. Check the PS5 IP if discovery fails.
- Slow/nonreading clients disconnect instead of accumulating unlimited queues
  or replaying stale frames. Reconnect waits for fresh random-access video.
- Stop acknowledges native cleanup before setup becomes editable. Only then can
  you replace/forget credentials. Forgetting an account also removes its paired
  console keys from this app's local credentials file. Historical Keychain entries
  are untouched. See [file protection limits](account-setup.md).
- Closing the window leaves capture running. Reopen from the app/Dock or
  **Window → Show Capture Window** (⌘0). Quit
  disconnects and joins workers without sleeping the console.

PS5 account/controller/TV restrictions are not bypassed. Continuous capture
through Mac sleep is unsupported. Measure real recovery and controller/TV
behaviour in the [final acceptance checks](validation.md).
