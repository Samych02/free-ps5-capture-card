# Free PS5 Capture Card

A standalone macOS PS5-to-OBS capture app, under development.

The app will forward compressed game video **and audio** together to OBS's Media
Source over localhost TCP using live Matroska. OBS decodes both tracks. There is
no app-side media decoding, encoding, preview, or speaker playback.

Planned controls include independent PSN sign-in/pairing, stream resolution/fps,
a 2–100 Mbps bitrate slider, H264 / H265 (default) / H265 HDR, and a configurable
local port (default 9295). HDR passthrough and audio are required scope.

## Current status

The native engine framework and SwiftUI development shell build in Debug and
Release. Tests cover the 118 upstream core cases, 3 engine-interface cases,
7 app/adapter cases, and 4 bootstrap cases. No PSN login, console pairing, or
PS5-to-OBS stream exists yet; Start Capture is intentionally disabled.
The installed Homebrew
libraries require macOS 26; macOS-15 distribution is not yet supported/verified.
See [the execution plan](docs/plans/free-ps5-capture-card.md) and
[validation evidence](docs/validation.md).

## Development prerequisites

Apple Silicon Mac, Xcode, Homebrew. Initial macOS deployment target is 15;
actual dependency minimums must be validated before claiming compatibility.

```sh
brew install cmake ninja xcodegen ffmpeg pkg-config openssl@3 protobuf@29 json-c miniupnpc libevent python@3.13
```

Then:

```sh
bash scripts/bootstrap.sh
bash scripts/build-native.sh --config Debug --tests
ctest --test-dir build/native --output-on-failure
python3 -m unittest discover -s Tests/Bootstrap -v
```

Build the app shell with `scripts/build.sh --config Debug`; its output is
`build/xcode/Build/Products/Debug/FreePS5CaptureCard.app`. Run native and app tests
with `scripts/test.sh --native --app`. Both commands print their underlying build
commands. Test filters currently support the native `interface` suite and app
classes `AppModelTests` / `EngineClientTests`; unknown filters are rejected.
Native filters `mux` and `tcp` run the new synthetic transport checks. The full
native suite includes those integration tests automatically.

## Synthetic OBS transport prototype

A no-transcode Matroska/TCP replay sender is now available independently of the
app shell. It sends both compressed video and Opus audio, including a validated
10-bit PQ/BT.2020 HDR fixture. See [run and OBS instructions](docs/obs-transport-prototype.md).
Automated FFmpeg-receiver checks pass; **actual OBS compatibility is still a
manual gate**, not a completed feature claim. This is not connected to a PS5 yet.

The shell uses a deliberately minimal native utility layout, supports system
appearance/accessibility, opens no playback devices, and does not auto-start
capture. Closing the window keeps the app alive; Quit joins engine cleanup
asynchronously. The framework still links local Homebrew libraries; the app is
not yet a self-contained distributable bundle.

Dependencies are fetched into ignored build directories, never into an existing
Chiaki checkout. Bootstrap uses an isolated Python environment and never installs
Python packages globally. Tools from Homebrew remain system prerequisites; their
versions are not yet a reproducible binary dependency lock.

This project is not affiliated with or endorsed by Sony. See LICENSE and
THIRD_PARTY_NOTICES.md before redistribution.
