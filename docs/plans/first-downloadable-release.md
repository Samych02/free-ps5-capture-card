# First downloadable release — living Execution Plan

## Goal and authorization

Prepare a downloadable Apple Silicon/macOS 26 app release for the accepted
0.1.0 build 8, with complete matching source alongside it. User explicitly chose
this over publishing a source-only preview. Keep behavior unchanged, including
the accepted controller/mic indicators. Work only on main; no extra branches,
external chiaki-ng checkout edits, credential access, Applications installation,
autostart, notarization or security-setting changes.

The existing GitHub draft is v0.1.0, currently targeting
200b5d0036416acb2d3a3e4bb652bedec800e540, with no assets and unpublished. Repository:
Samych02/free-ps5-capture-card; main is the only branch/default. Publishing is
authorized as the eventual goal, but matching-source completeness, independent
review and final device/clean-machine evidence are not thereby established.
Do not silently publish an unreviewed app or label missing gates passed.

## Progress

- [x] Push human-readable README; create verified unpublished draft prerelease.
- [x] User selected downloadable-app release preparation with matching source.
- [x] Inventory accepted bundle: 13 dylibs / 11 Homebrew formulae, static patched
  Chiaki/curl/nanopb/gf-complete/Jerasure and minimal FFmpeg 9.0.2.
- [x] Freeze exact accepted app/source provenance and file hashes; archive app
  separately without altering it or overwriting earlier artifacts.
- [x] Pin every source archive, formula/patch/resource and git/submodule revision
  needed by the actual shipped closure; obtain and hash-check content, not URLs
  alone. Explicitly resolve installed-formula versus current-API differences.
- [x] Add failing offline tests for release assembly/integrity, implement minimal
  fail-closed source assembly and verification tooling, rerun tests.
- [x] Assemble portable source archive with project source, core/submodule Git
  bundles or complete source trees, FFmpeg archive, exact Homebrew source/patches,
  recipes/notices, build/relink instructions and checksummed inventory.
- [x] Extract into a fresh isolated folder and verify retrieval/checksums, pinned
  Git restoration/patching and supported build paths. Record actual scope; a
  warm-host rebuild is not clean-machine evidence or bit-for-bit reproduction.
- [x] Refresh immutable binary/static/dynamic license/provenance inventory and
  review package against corresponding-source/relink requirements; independent
  license/code/security review is still a separate gate.
- [x] Run final automated regression/package checks and document outstanding
  manual/independent gates, artifacts/hashes and release notes. Never upload app
  assets or publish until the required release checks are satisfied.
- [ ] Obtain independent corresponding-source/license/code/security approval.
- [ ] Complete final clean-machine and calibrated/manual device acceptance.
- [ ] Only after those gates, attach the frozen app/source/manifest/checksums and
  publish the explicitly chosen release with accurate signing/limitation notes.

## Accepted inputs and boundaries

Accepted app: `build/package-mic-disconnected/Free PS5 Capture Card.app`, version
0.1.0 build 8. It was built during the a519325 checkpoint; later commits change
documentation/release metadata only. Verify relevant source-tree equivalence,
embedded Dependencies.lock.json and package notices before treating any current
source snapshot as matching. Do not substitute build 3/4/5/6/7 or rebuild new
behavior. Fresh timestamps/signatures are not proof of reproducibility.

The unrelated local deletion `docs/plans/file-credential-storage.md` must stay
unstaged/uncommitted. Export project source with git archive from an explicit
commit, never copy the working directory or build folder wholesale. Never read
the live credentials file, Keychain records, browser state or environment secrets.

The binary's `Contents/Resources/RuntimeNotices/runtime-manifest.json` identifies
the 13 actually bundled dylibs, not the historical broad Homebrew FFmpeg closure.
Formulae: json-c 0.19, miniupnpc 2.3.3, libevent 2.1.13_1, libidn2 2.3.8,
openssl@3 3.5.9, libnghttp2 1.71.0, libpsl 0.23.3, libssh2 1.11.1_7,
libunistring 1.5, gettext 1.0 and openssl@4 4.1.0. Both OpenSSL major versions
are actually shipped; do not minimize/replace them in this release-prep task.

Embedded FFmpeg capture-build.json records the runtime profile, source checksum,
compiler/SDK and flags. The pinned source archive is `build/ffmpeg-9.0.2.tar.xz`;
verify its bytes. No registered codecs/demuxers/protocols are added to the runtime.

## Design and file responsibilities

Source assembly is a local, explicit tool, never an automatic publisher or global
installer. Separate a checked-in immutable source-input manifest from generated
archives/caches under `build/release-preparation`. Inputs must bind exact names,
versions, URLs/revisions and SHA-256, plus component/patch relationships. An input
with unresolved provenance is a blocker, not a successful placeholder.

Use a small Python assembler under `scripts/`, with focused standard-library
unit tests under `Tests/Bootstrap/`. It should require explicit app, output,
source revision and manifest/cache arguments. It may fetch only pinned HTTPS
content, reject non-HTTPS redirects/checksum mismatches and reuse verified cache
files. Do not execute downloaded recipes to discover metadata. Refuse existing
output destinations, symlink/path traversal/archive-special-file surprises,
uncovered runtime libraries, mismatched app lock/build or missing source/patches.
Never fall back to a different version or certify legal compliance.

Preserve the exact formula Ruby files embedded in build 8. Current Homebrew API
metadata can supply archive/patch hashes only after comparing stable source,
revision and the complete patch list against those embedded recipes. It is not
itself proof of which historical bottle was built. Capture those distinctions
in the inventory rather than claiming all installed receipts match current HEAD.
For file-based patches, pin Homebrew/core commit + published file checksum.
Include source resources used on the supported macOS build; explicitly label
Linux-only test resources if included conservatively. Build tools/system SDKs
are not automatically app runtime components or redistributable source assets.

For Git-based Chiaki and pinned submodules, prefer offline Git bundles if needed
to preserve the existing strict revision/patch validation. Do not weaken
ApplyChiakiPatches.cmake or fabricate commit identity for an extracted tarball.
Restore each pinned repository/submodule locally, use explicit FetchContent
source override when proving an offline native build, and prove all five patches
apply. A plain GitHub project archive omits these upstream sources.

The source package must explain how to rebuild the app/core/FFmpeg, rebuild or
replace the bundled dynamic libraries and relink the static covered work, including
the actual recipes/patches/configuration. Xcode/macOS SDK and standard build tools
remain separately obtained prerequisites. Do not describe fresh Homebrew install
of today's formulas as reconstruction of the frozen binary's exact dependencies.

## Verification and execution order

Re-run explicit source preparation (choose a new output name if it already exists):

```sh
python3 scripts/prepare-release.py fetch --manifest docs/releases/v0.1.0-sources.json --cache build/release-preparation/cache
python3 scripts/prepare-release.py assemble --manifest docs/releases/v0.1.0-sources.json --cache build/release-preparation/cache --app "build/package-mic-disconnected/Free PS5 Capture Card.app" --repo . --core build/native/Release/_deps/chiaki-src --instructions docs/releases/rebuild-v0.1.0.md --output build/release-preparation/source-next
python3 scripts/prepare-release.py verify --directory build/release-preparation/source-next
```

These commands do not package/publish the binary automatically or certify review.
The frozen app tree must match, including resource bytes/link targets/file modes.

1. Freeze: validate embedded lock, full app inventory, version/signatures and
   source equivalence; no runtime behavior changes. `python3
   Tests/Support/check_bundle.py --app 'build/package-mic-disconnected/Free PS5 Capture Card.app'`
   must pass. Export only committed project files.
2. Inputs: use installed/embedded formulas and public package metadata, download
   exact archives/patches with expected hashes into a private build cache. Prove
   local libssh2 patches match checksum and apply in recipe order to its release.
3. TDD: add tests and run `python3 -m unittest discover -s Tests/Bootstrap -v`;
   observe focused failures before implementation. Cover missing/changed source,
   path/symlink attacks, incomplete runtime coverage, app/lock mismatch, output
   collision and checksum-list validation, with synthetic temporary fixtures.
4. Assemble into a fresh output tree and write inventory/README/hash list; package
   source and app separately. Round-trip extract safely and verify every declared
   payload. Exercise restoring pinned Git sources and CMake patch verification.
5. Rebuild verification must distinguish dependency reconstruction, native/app
   rebuild, relinking and clean-machine launch. Reuse supported build caches for
   normal tests, but do not use them as proof of independent source completeness.
6. Run Bootstrap/native/app/package checks at stable checkpoint. Record counts,
   command logs, artifact SHA-256, exact source/build revisions and unmet gates
   in `docs/releases/` and `docs/source-distribution.md`. Update GitHub draft notes
   honestly without publishing or uploading app assets prematurely.

## Surprises & Discoveries

- Accepted app ships 13 dylibs from 11 formulae, including OpenSSL 3 and 4.
- libssh2 1.11.1_7 uses 12 backport/security patches: six downloaded and six
  Homebrew file patches. Embedded recipes alone omit the six local patch bytes.
- Current Homebrew metadata has `local_patch_checksums` and tap commit
  690fc594cba046a7139f745558258a1e9495ae70. Installed receipts' source tap hash is
  null, and embedded/API Ruby checksums differ; archive/patch equivalence must
  be established rather than assuming identical formula provenance.
- Some bottle receipts reference older build-time dependency versions than the
  dylibs now linked into the app. Distinguish actual shipped library source from
  historical bottle-build provenance; do not claim bit-identical rebuilds.
- Existing core validation requires real Git commit identity. A tar archive with
  no Git metadata cannot simply bypass that guard for an offline source build.
- `git apply --check` rejected a libssh2 hunk that Homebrew's actual patch tool
  accepts. Verified installed Homebrew patch.rb uses `patch -g 0 -f -p1`; all 12
  exact pinned patches pass dry-run/application in that order with those semantics.
  Patch bytes were not changed to make the verification pass.
- Assembly tests additionally caught canonical-path aliases, unsupported patch
  directives, unlisted Mach-O frameworks/missing dylibs and unhandled macOS
  resources. Fixed fail-closed checks, preserving `resource(r)` accessors and
  excluding only explicit Linux resource declarations for the supported target.
- First round-trip source archive is 311,150,723 bytes. It restores all six Git
  repositories and rebuilds FFmpeg, core and the Release app in 145.24 s, using
  existing Homebrew libraries/build tools. This is not an all-dependency or
  clean-machine rebuild. Final archive will include the refined checker/guide.
- Fresh stable checks: Bootstrap 38/38; native 13/13 including transport (184.59 s);
  app 92/92; package 15 Mach-O checks; runtime FFmpeg policy 0 codecs, 2 parsers,
  1 muxer, 0 demuxers/protocols, LGPL version 2.1 or later. Logs under
  `build/release-preparation/final-*.log`.
- Final source checks add frozen-tree binding for all 78 app files/links and file
  modes, so altered resources or a same-version/different-binary app cannot pass.
  Final Bootstrap count is 39/39, including 12 release-specific tests.
- Final source archive has 260 files, 311,156,992 bytes, SHA-256
  d7bf8fc04245bc621b4087ce9f94d0f0db8a9e98b1b5da7f63a2e0d61d721bf2.
  App archive: 8,545,506 bytes, SHA-256
  dc2e44ec779d7029be646d785064bed0df40b024c53b8b7dcc4b24cb816150b1.
  All final Git bundles restore offline at pinned revisions; app ZIP extraction
  matches the frozen tree and binary checks. Final source code/upstream/notices
  are byte-identical to the successful development-host source rebuild.

## Decision Log

- Follow-up user instruction: "do the release. make the note human and readable
  easily". Publish the frozen app/source/manifest/checksums as a developer
  prerelease now, explicitly disclosing outstanding review/manual checks. This
  supersedes the earlier draft-only publication hold; no missing gate is passed
  or independently certified by this decision.

- User chose downloadable app preparation, not an empty/source-only first release.
- Keep accepted build 8 and its known icons unchanged; only release tooling,
  inventories and source-delivery artifacts are in scope.
- Include actual matching source, not a list of URLs or a speculative source offer.
- Hold the remote release in draft until source/review/manual gates are addressed;
  source-archive verification is engineering evidence, not independent legal review.
- Main remains the only branch. No binary uploads, default-branch changes or
  new platform/signing/security changes are hidden inside release preparation.

## Outcomes & Retrospective

Engineering preparation is complete: source assembler/tests and pinned inputs,
frozen app ZIP, 311 MB matching-input source archive, 19-component inventory,
checksums, modification/relink instructions, source round-trip/native/app build
and current regressions. See `docs/reviews/first-release-preparation.md` for exact
files/hashes/limitations and review questions. Accepted runtime behavior and old
packages are untouched; unrelated deleted credentials-design doc is uncommitted.

Blocked on the independent and final manual release gates above. The agent cannot
independently approve its own work or establish clean-machine/HDR/sync/soak results
from these host checks. Keep the GitHub draft unpublished and without uploaded app
assets; update its notes with prepared artifacts and actual remaining blockers.
