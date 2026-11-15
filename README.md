# Free PS5 Capture Card

A native macOS utility forwarding compressed PS5 **video and audio** to one OBS
Media Source over localhost TCP/Matroska. OBS decodes both tracks and performs
any final encoding. No app preview, local sound, controller, microphone, media
decoder, encoder or tone mapper.

Built on the Remote Play core from **[chiaki-ng](https://github.com/chiaki-ng/chiaki-ng)**,
which continues **[Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki)**. Thanks to the
original Chiaki authors, chiaki-ng maintainers and all upstream contributors.
This is a separate capture-focused project, not an official Chiaki/chiaki-ng release.

Unofficial project; not affiliated with Sony Interactive Entertainment or the OBS Project.

**0.1.0 release candidate — final acceptance is pending.** Earlier builds established
browser login, real pairing and all three synthetic feeds in OBS.
Real PS5-to-OBS sync, HDR output, long-run reliability and independent review
are not yet verified.

**Credential storage:** account and PS5 keys use an unencrypted, owner-only local
file, not Keychain. Historical Keychain records are never read, migrated or
deleted. This UI update does not reset existing file-based credentials; users
coming from a Keychain-only build must sign in and pair once. Current-candidate
file-backed real-device persistence remains a manual acceptance check.

## Use

Apple Silicon, **macOS 26+** for the current Homebrew-based build.

1. [Sign in](docs/account-setup.md) and [pair your PS5](docs/pairing.md).
2. Choose H264, **H265 (Default)**, or H265 HDR; 720p/1080p; 30/60 fps;
   2–100 Mbps; and a local port (default 9295).
3. Start Capture. In OBS add one Media Source, turn Local File off, paste
   `tcp://127.0.0.1:9295`, and choose input format `matroska`.

The media endpoint is unauthenticated: other local users/processes can connect.
It is not encrypted or restricted to OBS. Do not expose or forward the port.

In **OBS connection**, **Show metrics** expands all received-media, queue and clock
statistics; **Hide metrics** collapses them. The choice is remembered, starting
hidden. Current health and Start/Stop stay at the top; historical reset/retry
messages appear only under **Recent events** inside metrics. Expand **Manage
account and pairing** to change an existing PS5 or account. Closing the window
keeps capture running; use **Window → Show Capture Window** (⌘0) to reopen it.

See the [first-release checklist](docs/release-checklist.md) for what is verified,
what still needs hands-on acceptance, and distribution limitations.

See [setup and OBS instructions](docs/setup.md), [architecture](docs/architecture.md),
and [validation / pending gates](docs/validation.md). HDR never silently falls
back. Stop before changing setup. Closing the window does not stop capture;
Quit disconnects asynchronously.

## Local credentials

Account identity and PS5 pairing keys are stored at
`~/Library/Application Support/FreePS5CaptureCard/credentials.json`.
The directory is 0700 and the file is 0600. This prevents access by other OS
users, **not processes running as your macOS user**; it is not app-level
encryption. Do not share or commit this file. OAuth tokens, redirect URLs,
pairing/login codes and Sony passwords are not saved. Forget Account removes
the local record; it does not touch historical Keychain entries or erase backups.
The app also checks extended ACLs on its private directory, lock and credential
files. Granting ACLs (including inherited grants) are rejected rather than silently
removed; deny-only ACLs are permitted. See [storage troubleshooting](docs/account-setup.md).

## Build and verify

Install Xcode (with its command-line tools) and Homebrew, then install the build
prerequisites. `bootstrap.sh` checks them; it does not install Homebrew formulae.

```sh
brew install cmake ninja xcodegen ffmpeg pkg-config openssl@3 protobuf@29 json-c miniupnpc libevent python@3.13
scripts/bootstrap.sh
scripts/build.sh --config Debug
scripts/test.sh --native --app
python3 -m unittest discover -s Tests/Bootstrap -v
```

App: `build/xcode/Build/Products/Debug/FreePS5CaptureCard.app`.
Native builds use separate `build/native/Debug` and `build/native/Release` trees.
The first build also source-builds a SHA-256-pinned minimal FFmpeg into a private
`build/dependencies/ffmpeg` cache. It is reused across configurations; there is
no global install or runtime fallback to Homebrew FFmpeg. Homebrew `ffmpeg` and
`ffprobe` are still needed as external synthetic-test tools.
Xcode and packaging select the matching framework automatically. Each new tree
has a one-time cold build; switching configurations then reuses its own compiled
objects. Older artifacts directly under `build/native` are left untouched but
are no longer used by these scripts. Explicit CTest runs should select
`--test-dir build/native/Debug` (or `Release`).
`scripts/test.sh --ui` also needs a logged-in desktop and working Xcode UI
automation permission. This currently fails to enable on the development host.
Automated tests never sign in or contact a real PS5.
If Xcode's IDE test-session connection also stalls, the explicit unit-only
alternative is `bash scripts/test-app-unit-direct.sh`. It loads the Debug test
bundle directly with a minimal environment; it does not test app launch or UI
and does not silently replace a failed normal test run.

Native filters: `interface`, `pairing`, `media-sink`, `timeline`, `queue`,
`codec-units`, `session`, `diagnostics`, `mux`, `tcp`.
`python3 Tests/Support/integration.py --build-dir build/native/Debug --case faults`
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

## License and upstream credit

New project code is **GNU AGPL-3.0-only**; see [LICENSE](LICENSE). This is a
copyleft project, not MIT/BSD or a proprietary wrapper. Upstream files retain
their original copyright notices, license terms and applicable OpenSSL exception
in [LICENSES/AGPL-3.0-only-OpenSSL.txt](LICENSES/AGPL-3.0-only-OpenSSL.txt).
The exception is not a blanket relicensing of other dependencies.

- **Chiaki**, originally by Florian Märkl and contributors, provides the foundation
  for the Remote Play implementation.
- **chiaki-ng**, maintained by Street Pea and contributors, provides the pinned
  core used here: revision `43728af2e639fecb8a7af1cd68c29820a40de973`.
- PSN account identification adapts chiaki-ng's flow, whose original account-ID
  code credits **grill2010**.
- **FFmpeg** provides compressed-packet parsing and Matroska muxing, not app-side
  playback. Current builds use a pinned minimal static configuration, not the
  broad Homebrew runtime shipped in earlier candidates. Its actual feature and
  license checks are tracked separately; the project itself remains AGPL.

Our changes include the SwiftUI utility, private credential-file storage,
Chiaki encoded-media callbacks and bounds/lifetime fixes, and the two-track
localhost output/timing/recovery pipeline. The upstream patches and their hashes
are tracked in [Dependencies.lock.json](Dependencies.lock.json).

See [third-party notices](THIRD_PARTY_NOTICES.md) and the
[source-distribution checklist](docs/source-distribution.md) for dependency terms
and outstanding corresponding-source requirements. **Public binary distribution
is not cleared:** the current bundle contains notices/recipes, not complete
corresponding source. Attribution alone does not satisfy those obligations.

## Future platforms

Only **Apple Silicon/macOS 26** is built and supported today. Native networking,
compiler arithmetic and packaging boundaries are isolated for later Intel Mac,
Linux and Windows work; this is not a Universal 2 or cross-platform release.
See [portable foundations](docs/portability.md) for the extension points and tests
required before adding a target. The native SwiftUI interface remains unchanged.
