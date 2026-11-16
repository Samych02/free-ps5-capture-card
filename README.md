# Free PS5 Capture Card

Get your PS5's picture and game audio into OBS on your Mac—without an HDMI capture
card. Play on your TV as usual, and use OBS to record or stream.

The app uses Remote Play to send compressed video and audio to a single OBS Media
Source. It is capture-only: no game controls, microphone capture, preview or local
playback. OBS handles decoding and any final encoding.

Built on **[Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki)** and
**[chiaki-ng](https://github.com/chiaki-ng/chiaki-ng)**. This is an independent,
unofficial project, not affiliated with Sony, OBS or the upstream projects.

## What you need

- An **Apple Silicon Mac running macOS 26 or later**. Intel Macs, Windows and
  Linux are not supported by the current build.
- A PS5 with **Settings → System → Remote Play → Enable Remote Play** turned on.
- OBS on your Mac, with the Mac and PS5 on the same local network. Wired Ethernet
  is recommended for a more reliable connection.

This uses Remote Play, not a direct HDMI feed, so your network affects quality and
latency. There is no internet/WAN connection mode in this app.

## Get started

[Download the Mac developer preview](https://github.com/Samych02/free-ps5-capture-card/releases/tag/v0.1.0)
and unzip it, or [build from source](#build-from-source), then:

1. Open the app, [sign in to PSN](docs/account-setup.md) and
   [pair your PS5](docs/pairing.md).
2. Start with **H265, 1080p, 60 fps and 15 Mbps**. Click **Start Capture**.
3. In OBS, add one **Media Source** and turn **Local File** off. Set its input to
   `tcp://127.0.0.1:9295` and its input format to `matroska`.
4. Check for moving video and activity in that source's **OBS audio mixer**.

H264 and H265 HDR are also available, along with 720p, 30 fps and adjustable
bitrate. HDR needs a matching PS5/OBS setup; the app will not silently switch to
SDR or tone-map the picture. See the [full setup guide](docs/setup.md).

**Closing the window does not stop capture.** Use **Stop** or quit the app.
Reopen the window with **Window → Show Capture Window** (⌘0). Stop capture before
changing settings. For troubleshooting, open **Show metrics → Recent events**.

## Current status and limitations

The current developer version is **0.1.0, build 8**. It has been reported working
in hands-on PS5 testing. This is an early developer preview, not a finished or
notarized app; macOS may block it from opening.

- The PS5 still shows controller/microphone indicators, even though the app sends
  no controller input or microphone audio. An extra controller slot was observed
  in FIFA; that issue is not confirmed resolved. The current behavior has been
  accepted for this developer version, and further experiments are paused.
- HDR output, precise audio/video sync and long-session reliability still need
  final validation, along with independent code, security and license review.
- Protected content cannot be captured. The app respects Remote Play restrictions.
- Only one OBS connection is supported at a time.

See the [release checklist](docs/release-checklist.md) and
[test results](docs/validation.md) for the details. The controller/mic investigation
is recorded separately in the [technical notes](docs/reviews/capture-only-negotiation.md).

## Privacy and local access

Your account identity and PS5 pairing keys are saved in an **unencrypted local
file**, protected by owner-only permissions:

`~/Library/Application Support/FreePS5CaptureCard/credentials.json`

Other programs running as your macOS user may still read it. Do not share or
commit this file. Sony passwords, OAuth tokens and pairing/login codes are not
saved. The app does not use Keychain or read, migrate or delete old Keychain
records. See [account setup and storage details](docs/account-setup.md).

The OBS feed is local, but **not authenticated or encrypted**: other users and
programs on your Mac can connect to it. Do not expose or forward the port.

## Build from source

Install Xcode with its command-line tools and Homebrew. From the repository root:

```sh
brew install cmake ninja xcodegen ffmpeg pkg-config openssl@3 protobuf@29 json-c miniupnpc libevent python@3.13
scripts/bootstrap.sh
scripts/build.sh --config Debug
```

Open `build/xcode/Build/Products/Debug/FreePS5CaptureCard.app`.

The first build also compiles a pinned, minimal FFmpeg library into a private
cache; later builds reuse it. Homebrew's FFmpeg is used for test tools, not as the
app's media runtime. Nothing is installed into Applications automatically.

### Run tests

```sh
scripts/test.sh --native --app
python3 -m unittest discover -s Tests/Bootstrap -v
```

Automated tests use synthetic data and do not sign in or contact a real PS5.
UI tests (`scripts/test.sh --ui`) need a logged-in desktop and Xcode automation
permissions. If the IDE test runner cannot connect, an explicit unit-only runner
is available: `bash scripts/test-app-unit-direct.sh`. It does not test app launch
or interactive UI.

### Make a local developer bundle

```sh
scripts/package-dev-app.sh --config Release --output build/package
python3 Tests/Support/check_bundle.py --app "build/package/Free PS5 Capture Card.app"
```

This bundles the runtime libraries and notices and applies ad-hoc developer
signatures. It does not notarize, publish or install the app, and it will not
overwrite an existing output app. See [dependency and bundle maintenance](docs/upstream-maintenance.md).

## Credits and license

This project would not exist without **Chiaki**, created by Florian Märkl and
contributors, and **chiaki-ng**, maintained by Street Pea and contributors. The
PSN account flow also builds on their work and credits **grill2010** for the
original account-ID code. **FFmpeg** provides compressed-media parsing and
Matroska output—not app-side playback.

New project code is licensed under **GNU AGPL-3.0-only**; see [LICENSE](LICENSE).
Upstream code keeps its original notices and terms, including the applicable
[Chiaki OpenSSL exception](LICENSES/AGPL-3.0-only-OpenSSL.txt). Dependency revisions
and patch hashes are recorded in [Dependencies.lock.json](Dependencies.lock.json).

The release includes a separate source archive with dependency sources, patches
and rebuild instructions. Independent license review is still outstanding.
Before redistributing, read the
[third-party notices](THIRD_PARTY_NOTICES.md) and
[source-distribution checklist](docs/source-distribution.md).

For the internals, see the [architecture](docs/architecture.md) and
[future platform foundations](docs/portability.md).
