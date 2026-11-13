# Free PS5 Capture Card

A native macOS utility forwarding compressed PS5 **video and audio** to one OBS
Media Source over localhost TCP/Matroska. OBS decodes both tracks and performs
any final encoding. No app preview, local sound, controller, microphone, media
decoder, encoder or tone mapper.

**Development build — live acceptance is pending.** The user confirmed browser
login, real pairing, Keychain recovery and all three synthetic feeds in OBS.
Real PS5-to-OBS sync, HDR output, long-run reliability and independent review
are not yet verified.

## Use

Apple Silicon, **macOS 26+** for the current Homebrew-based build.

1. [Sign in](docs/account-setup.md) and [pair your PS5](docs/pairing.md).
2. Choose H264, **H265 (Default)**, or H265 HDR; 720p/1080p; 30/60 fps;
   2–100 Mbps; and a local port (default 9295).
3. Start Capture. In OBS add one Media Source, turn Local File off, paste
   `tcp://127.0.0.1:9295`, and choose input format `matroska`.

See [setup and OBS instructions](docs/setup.md), [architecture](docs/architecture.md),
and [validation / pending gates](docs/validation.md). HDR never silently falls
back. Stop before changing setup. Closing the window does not stop capture;
Quit disconnects asynchronously.

## Build and verify

Install Xcode and Homebrew, then:

```sh
scripts/bootstrap.sh
scripts/build.sh --config Debug
scripts/test.sh --native --app
python3 -m unittest discover -s Tests/Bootstrap -v
```

App: `build/xcode/Build/Products/Debug/FreePS5CaptureCard.app`.
`scripts/test.sh --ui` also needs a logged-in desktop and working Xcode UI
automation permission. This currently fails to enable on the development host.
Automated tests never sign in or contact a real PS5.
If Xcode's IDE test-session connection also stalls, the explicit unit-only
alternative is `bash scripts/test-app-unit-direct.sh`. It loads the Debug test
bundle directly with a minimal environment; it does not test app launch or UI
and does not silently replace a failed normal test run.

Native filters: `interface`, `pairing`, `media-sink`, `timeline`, `queue`,
`codec-units`, `session`, `diagnostics`, `mux`, `tcp`.
`python3 Tests/Support/integration.py --build-dir build/native --case faults`
exercises local recovery. Synthetic encoders/decoders are test tools only.
Dependencies are fetched into ignored build folders, never an existing Chiaki
checkout. Homebrew binary builds are not a reproducible binary lock.

## Developer bundle

```sh
scripts/package-dev-app.sh --config Release --output build/package
python3 Tests/Support/check_bundle.py --app "build/package/Free PS5 Capture Card.app"
```

This copies recursive runtime libraries/notices, rewrites install names and
ad-hoc signs inside-out. Existing output apps are never overwritten. It is **not**
a notarized release, installation, publishing or clean-machine onboarding proof.
See [maintenance](docs/upstream-maintenance.md) and
[third-party notices](THIRD_PARTY_NOTICES.md) before redistribution.

Not affiliated with or endorsed by Sony. New code is AGPL-3.0-only; applicable
upstream licenses and exception text are preserved in `LICENSES/`.
