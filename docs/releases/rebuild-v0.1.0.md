# Rebuild or relink the build-8 developer app

This accompanies the source payload for Free PS5 Capture Card 0.1.0 build 8.
It is an engineering reconstruction guide, not a bit-for-bit reproduction claim
or independent license/security approval. The app remains Apple Silicon/macOS 26.

## Modification notice

This is a modified capture-focused application by the Free PS5 Capture Card
contributors, based on Chiaki/chiaki-ng. The build-8 project snapshot is
`a519325dcd5b345b8fa1758bf1d06cb620bdda6d`, committed **2026-11-16**. Original
upstream copyright/license notices are preserved; they are not replaced by this
notice or relicensed wholesale under a new exception.

The five patches in `project/patches/chiaki/` implement the embedding deployment
target, encoded-media callbacks/receiver bounds/lifetime and identity fixes,
checked control-frame parsing, capture-only input/feedback policy with the
explicit negative mic declaration, and the bounded initial-control formatter
that retains ControllerType but omits PadProcNo. The project includes its own
SwiftUI interface, owner-only credential-file handling and compressed two-track
OBS output/recovery pipeline. Patch files identify the actual modified upstream
paths; `Dependencies.lock.json` binds their exact bytes.

FFmpeg source is unmodified; its minimal static configuration is recorded with
the bundle. Homebrew's libssh2 source is modified by the 12 upstream/backport
patches supplied in order. Their original authors/dates and Homebrew's adaptation
notes are preserved in the patch headers and archived recipe; those changes are
not claimed as new work by this project. Further independent review of the full
modification-notice/source-delivery arrangement remains a release requirement.

## Contents and verification

- `project/`: committed app/core-adapter/build/patch source at the revision in
  `source-inventory.json`. No working-tree or live credential files are copied.
- `git/`: full offline Git bundles of pinned Chiaki and its required submodules.
- `upstream/`: actual FFmpeg and Homebrew upstream source archives, plus all 12
  libssh2 patches in the exact order in `source-inputs.json`.
- `notices/`: exact recipe/license/build-configuration files from the accepted
  app. The FFmpeg configure/compiler/SDK record is under
  `notices/project/EmbeddedSources/ffmpeg/capture-build.json`.
- `tools/prepare-release.py`: assembly/integrity utility; never publishes or installs.
- `SHA256SUMS`: hashes of every payload file, including inventories and tools.
- `app-inventory.json`: exact accepted app file hashes, symlink targets and file
  modes, bound by `source-inputs.json`. This binds the binary tree, not just its
  version number; it is still author provenance, not independent attestation.

Before executing source/build instructions, verify the **outer archive hash**
against the release's separately published checksum list. Then extract into an
empty directory and, from this source-payload root, run:

```sh
python3 tools/prepare-release.py verify --directory .
```

This detects missing/changed/extra files. An unsigned checksum does not prove the
publisher's identity, license compliance or source/binary equivalence by itself.
Do not put build outputs inside this verified tree: that would add unchecked files.

## Prerequisites

Use an Apple Silicon Mac with macOS 26+, Xcode and its command-line tools, Git,
CMake 3.27+, Ninja, XcodeGen, pkg-config and Python. The original build used the
compiler/SDK recorded with FFmpeg; an available compatible Xcode is required.
Xcode, the macOS SDK, system frameworks and ordinary build tools are not bundled.

The native protobuf generator requires Python protobuf 5.29.6 and `protoc` 29.
The project's `scripts/bootstrap.sh` prepares its isolated Python environment,
but requires Homebrew prerequisites and may contact the Python package index.
Homebrew ffmpeg/ffprobe are external synthetic-test tools, not the app runtime.
Those tool installations are not part of an offline-runtime-source claim.

## Restore the pinned Git sources without network access

Choose a new working directory **outside** this verified source tree. Set
`SOURCE_WORK` to its absolute path before running the following Python block.
The destination must not already exist. It restores only the bundles named in
the verified inventory, disables network Git transports and checks every commit.

```sh
export SOURCE_WORK="$HOME/ps5-source-rebuild"
python3 - <<'PY'
import json, os, pathlib, subprocess
source = pathlib.Path.cwd().resolve()
work = pathlib.Path(os.environ['SOURCE_WORK']).expanduser().resolve()
work.mkdir(parents=True, exist_ok=False)
env = dict(os.environ, GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull,
           GIT_ALLOW_PROTOCOL='file', GIT_TERMINAL_PROMPT='0')
for key in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE', 'GIT_CONFIG_COUNT', 'GIT_CONFIG_PARAMETERS'):
    env.pop(key, None)
for entry in json.loads((source/'source-inventory.json').read_text())['git_sources']:
    target = work/'chiaki'
    if entry['checkout'] != '.':
        target = target/entry['checkout']
    subprocess.run(['git', '-c', 'core.hooksPath=/dev/null', '-c', 'advice.detachedHead=false',
                    'clone', '--no-hardlinks', str(source/entry['path']), str(target)], env=env, check=True)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=target, env=env, text=True).strip()
    assert commit == entry['revision'], entry['checkout']
print('Restored pinned upstream source:', work/'chiaki')
PY
```

Copy `project/` to another new directory under your workspace for the app build.
The core bundles contain pristine pinned source. On configure, the app's strict
patch validator applies the five source patches and checks their locked digests;
do not disable that validator or invent a replacement Git revision.

## Rebuild the non-system dynamic libraries

The payload includes upstream source for all 11 formulae responsible for the 13
shipped dylibs. Read the exact archived Ruby recipe and upstream build/installation
instructions before compiling each component. Preserve their patches, resources,
notices and data files. A fresh `brew install` using today's formula is **not**
reconstruction of these frozen source inputs.

Build into new, separate private prefixes rather than modifying a working app or
global Homebrew installation. Use arm64, `MACOSX_DEPLOYMENT_TARGET=26.0` and the
selected Xcode SDK. Set configure `--prefix`, CMake `CMAKE_INSTALL_PREFIX`, compiler
include/library paths and pkg-config search paths to those private prefixes.

The archived recipes provide the full procedure, including macOS conditions.
Their important component-specific settings are summarized here; ordinary
configure/CMake installation-prefix options still apply:

| Source component | Build settings / dependency ordering |
| --- | --- |
| OpenSSL 3.5.9 and 4.1.0 | Build separately with `perl Configure darwin64-arm64-cc`, `enable-ec_nistp_64_gcc_128`, `no-ssl3`, `no-ssl3-method`, `no-zlib`, explicit `--prefix`, `--openssldir`, `--libdir=lib`; then make/install. See each recipe for version-specific details. |
| json-c 0.19 | CMake Release build, `BUILD_APPS=OFF`, then build/install. |
| miniupnpc 2.3.3 | `make INSTALLPREFIX=<private-prefix> install`. |
| libunistring 1.5 | `am_cv_func_iconv_works=yes`, configure `--disable-silent-rules`, make/install; the recipe skips its failing macOS iconv checks. |
| gettext 1.0 | After json-c/libunistring. Preserve Clang warning workaround and `am_cv_func_iconv_works=yes`. Configure with the recipe's included glib/libcroco/gettext, libunistring prefix, Emacs/lisp directory and disabled Java/C#/Git/CVS/xz settings. Install without parallel make. |
| libidn2 2.3.8 | After libunistring/gettext. Configure `--disable-silent-rules --with-packager=Homebrew --with-libintl-prefix=<gettext-prefix>` and make/install. |
| libnghttp2 1.71.0 | Configure `--enable-lib-only`, then `make -C lib` and `make -C lib install`. |
| libpsl 0.23.3 | Configure `--disable-silent-rules --enable-builtin --enable-runtime=libicucore` on supported macOS; make/install. Includes its source data notices. |
| libevent 2.1.13_1 | Configure `--disable-debug-mode`, make/install; direct its OpenSSL discovery to the OpenSSL 4 prefix. |
| libssh2 1.11.1_7 | Apply **all 12** listed patches, in order, with strip level 1. Configure `--disable-silent-rules --disable-examples-build --with-openssl --with-libz --with-libssl-prefix=<openssl-4-prefix>`; make/install. |

The OpenSSL recipe's Perl test resources conditional on `on_linux` are not used
by this macOS build and are not included. No Linux build is provided.

Do not let a generic prefix search accidentally replace OpenSSL 3 with 4 or vice
versa. The core/curl uses the OpenSSL 3 prefix; libevent/libssh2's archived recipes
select OpenSSL 4. Both major source versions are included because both are shipped.
The original Homebrew bottle receipts can refer to older build-time dependency
versions than the dylibs currently shipped; this payload does not claim to
reproduce historical bottle bytes or their complete CI environment.

For libssh2, `source-inputs.json` lists archive first, then patches in application
order. Every patch is present, including the six `Patches/libssh2/*.patch` files
that are referenced but not embedded in the archived Ruby recipe. Do not drop
those files or substitute newer upstream HEAD commits.

## Rebuild FFmpeg, core and app

Seed the copied project's `build/ffmpeg-9.0.2.tar.xz` with the verified archive
from `upstream/ffmpeg/`. `python3 scripts/build-ffmpeg.py` builds the pinned static
runtime in the project's private cache and prints its install prefix. It verifies
the source checksum, never installs globally and never falls back to system
FFmpeg. It has no registered codecs, demuxers or network protocols.

Configure the copied app source with an explicit restored Chiaki directory and
your chosen private dependency/tool prefixes. Replace the example shell variables
with absolute paths. This is a template for those deliberately selected paths,
not an instruction to use arbitrary newly updated Homebrew libraries:

```sh
cmake -S "$APP_SOURCE" -B "$APP_SOURCE/build/native/Release" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 \
  -DFETCHCONTENT_SOURCE_DIR_CHIAKI="$SOURCE_WORK/chiaki" \
  -DOPENSSL_ROOT_DIR="$OPENSSL3_PREFIX" \
  -DCMAKE_PREFIX_PATH="$DEPENDENCY_PREFIXES" \
  -DCAPTURE_FFMPEG_ROOT="$FFMPEG_PREFIX" \
  -DPROTOC="$PROTOC" -DPYTHON_EXECUTABLE="$BUILD_PYTHON"
cmake --build "$APP_SOURCE/build/native/Release" --parallel
```

Supply pkg-config paths for your rebuilt json-c, miniupnpc and libevent where
needed. Preserve the original curl/static-core CMake options from the restored
source. Build the native `CaptureEngine.framework` first, then generate the Xcode
project with XcodeGen and build its FreePS5CaptureCard Release scheme, using
`project/project.yml` and `project/scripts/build.sh` as the recipe. The standard
script selects installed Homebrew prefixes; keep your explicit native cache
settings/private prefixes when doing a dependency-reconstruction experiment.
Do not claim that using host libraries rebuilt those libraries from this source.

After the native framework is built with those explicit inputs, the app build
itself can be run without the convenience script reselecting Homebrew prefixes:

```sh
cd "$APP_SOURCE"
xcodegen generate --spec project.yml
xcodebuild -project FreePS5CaptureCard.xcodeproj -scheme FreePS5CaptureCard \
  -configuration Release -derivedDataPath build/xcode \
  -destination 'platform=macOS,arch=arm64' build
```

## Replace a library or relink modified source

The complete first-party app/core adapter, patched Chiaki, curl, required FEC/
protobuf sources and FFmpeg library source/build recipe are supplied so covered
static code can be modified and the app relinked—not just re-signed unchanged.
Rebuild the private static FFmpeg/core archives and native framework, then rebuild
the Swift app against that framework. Do not link a decoder/encoder profile into
the app as a metadata workaround.

For a **modified** FFmpeg build, create your own source archive and update its
SHA-256 (and version/directory naming if changed) in your copy of
`Dependencies.lock.json`. Seed that archive at the corresponding build path.
The builder's identity includes the source/recipe/toolchain, so it creates a new
private prefix. A cached `capture-build.json`/archive is not recompiled merely
because a file inside its extracted source was edited. Do not fake the original
hash or disable the prefix/profile guard: use the manifest for your modified
source and point CMake at that new prefix. For modified Chiaki, export your own
patch changes, update their locked digests and restore a fresh pinned Git source
before applying that modified patch series. All of these build scripts/guards are
provided as modifiable source; there is no publisher signing key needed to relink.

For dynamic replacements, rebuild compatible arm64 dylibs, point the native
linker/packager to their private prefixes and create a **new** app bundle. Inspect
all dependent install names/rpaths, preserve applicable notices and source access,
then apply new local ad-hoc signatures inside-out. The developer packaging recipe
and `Tests/Support/check_bundle.py` show the runtime-closure/signing checks; they
do not grant legal clearance. No signing secret or locked installation mechanism
is required by this developer build. Never disable Gatekeeper/system protections.

Use the new bundle only for your own validation until applicable redistribution
obligations and the project's independent/manual release gates are satisfied.
Do not overwrite a working build or treat a successful local rebuild as proof of
clean-machine launch, calibrated HDR/sync or release readiness.
