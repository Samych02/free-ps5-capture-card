# Set up PS5 capture in OBS

## Prerequisites

- Current developer build: Apple Silicon and macOS 26+. No installer, autostart,
  notarization or compatibility promise for older macOS versions.
- Enable PS5 Settings → System → Remote Play. Use a reachable local console,
  preferably wired Ethernet. There is no WAN/hole-punching feature.
- [Sign in in your browser](account-setup.md), then [pair the PS5](pairing.md).
  Account and console keys use an unencrypted, owner-only local file, not Keychain.
  The new build requires fresh sign-in/pairing; no old credentials are imported.
  No settings export from another app.
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
- If the same PS5 gets a new IP, stop capture and choose **Change IP…**. Enter
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

- **Waiting for OBS and both tracks** does not claim OBS decoded anything.
  **Sending audio and video** means both compressed tracks were written.
- Diagnostic builds show **OBS resets** separately from **source retries**, plus
  a retained **Last OBS reset:** message. This distinguishes peer closure, stalled
  writes, queue storage/freshness, missing tracks and startup-keyframe waits. The
  output counter/history belong to the current PS5 source session, not all app
  launches. The message stays visible after an OBS reconnect and is not a new
  fatal error. Ordinary Stop does not deliberately count as an output fault.
  For repeated drops, report that exact line and the selected mode/bitrate—never
  credentials or media payloads. No timeout or buffering limits were relaxed by
  this instrumentation.
  Newer diagnostic builds distinguish **startup alignment failed** from an
  **audio delivery gap**, **video delivery gap**, missing configuration or both
  tracks. Packet ages are retained from the reset instant (or time since OBS
  attachment if that track never entered the queue). **audio-start** is a startup
  timestamp comparison, not a measured synchronization error. Include the entire
  message when reporting a drop.
  Clock-diagnostic builds also show **Clock drift vs ingress** and **Observed
  counters**. Positive drift means counter-derived duration advances faster than
  observed receipt time; negative means slower. Rates use unwrapped counter
  advances, not decoded/displayed FPS or simply the number of callbacks. They
  include source counter gaps, exclude rejected duplicates/reordering, and span
  the current PS5 session even when OBS reconnects. Network jitter can affect
  these values; they are not measured A/V sync or hardware-clock accuracy.
  Report both clock lines together with the reset message after a drop.
  The clock-correction build explicitly maps video ingress timing onto the audio
  sample clock instead of deriving video timestamps from nominal SPS FPS. The
  **Nominal counter drift vs ingress** line remains diagnostic: a negative video
  value is not itself a failure after this correction. Watch the actual output
  state and OBS reset count. No decoder or separate audio source is introduced,
  and real synchronization still needs verification.
- Supported game/HDR metadata changes now keep the PS5 session connected.
  Identical configuration updates and valid HDR metadata updates should also keep
  the existing OBS connection. Actual video parameter-set changes may briefly
  show **Waiting for OBS** while only the Matroska output is refreshed; **source
  retries** should not increase for these supported changes. Audio capture and
  its sample clock continue, though an OBS output reset temporarily interrupts
  delivery of both tracks on that single source.
  Video counter diagnostics begin a new segment after video reconfiguration;
  this does not reset the source audio clock. A genuine unsupported format or
  HDR-to-SDR switch while HDR is selected still reports an error, not a hidden
  fallback. Keep the requested codec mode unchanged for the transition test.
- A game transition can temporarily stop one or both media tracks. After a long
  pause the app waits for fresh audio and video without restarting Remote Play;
  OBS output can reconnect with a fresh timing epoch. Clock diagnostics restart
  after this real-gap recovery, but **source retries** should not increase from
  media silence alone. If a genuine source reconnect does happen, its most recent
  initiating failure remains visible after recovery. Stop clears that history.
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
- Closing the window leaves capture running. Reopen from the app/Dock. Quit
  disconnects and joins workers without sleeping the console.

PS5 account/controller/TV restrictions are not bypassed. Continuous capture
through Mac sleep is unsupported. Measure real recovery and controller/TV
behaviour in the [final acceptance checks](validation.md).
