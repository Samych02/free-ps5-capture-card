# Free PS5 Capture Card

A standalone macOS PS5-to-OBS capture app, under development.

The app will forward compressed game video **and audio** together to OBS's Media
Source over localhost TCP using live Matroska. OBS decodes both tracks. There is
no app-side media decoding, encoding, preview, or speaker playback.

Planned controls include independent PSN sign-in/pairing, stream resolution/fps,
a 2–100 Mbps bitrate slider, H264 / H265 (default) / H265 HDR, and a configurable
local port (default 9295). HDR passthrough and audio are required scope.

## Current status

Native dependency baseline builds, with 118/118 upstream core tests passing.
No capture interface or PS5-to-OBS stream exists yet. The installed Homebrew
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

Dependencies are fetched into ignored build directories, never into an existing
Chiaki checkout. Bootstrap uses an isolated Python environment and never installs
Python packages globally. Tools from Homebrew remain system prerequisites; their
versions are not yet a reproducible binary dependency lock.

This project is not affiliated with or endorsed by Sony. See LICENSE and
THIRD_PARTY_NOTICES.md before redistribution.
