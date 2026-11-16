# First downloadable release — engineering handoff

## Status

The user selected a downloadable Mac app release with matching source, not a
source-only preview. Engineering candidates are being assembled for accepted
**0.1.0 build 8**; app behavior, known PS5 controller/mic indicators and prior
packages are unchanged. The GitHub v0.1.0 release remains an unpublished draft.
This document is author verification and a request for independent review,
**not independent code/security/license approval or permission to skip gates**.

## Frozen app and matching inputs

- App: `build/package-mic-disconnected/Free PS5 Capture Card.app`.
- Production-source snapshot: `a519325dcd5b345b8fa1758bf1d06cb620bdda6d`
  (2026-11-16). App/Native/cmake/patch/dependency/build-entrypoint equivalence to
  the current source was checked before assembly. Later prep changes are tooling
  and documentation, not changes to the accepted runtime.
- App inventory: 78 files/symlinks, 15 Mach-O files including app/engine and 13
  dylibs. Individual bytes/symlink targets are frozen in
  `build/release-preparation/build-8-inventory.json`.
- The new source-input manifest is `docs/releases/v0.1.0-sources.json`: 11 exact
  Homebrew archive inputs, minimal FFmpeg 9.0.2 and all 12 libssh2 patches. There
  are **24** actual pinned download payloads; all SHA-256 checks pass.
- Original embedded formula recipes match pinned recipe hashes. Stable source
  hashes and the complete patch sequence are cross-checked, rather than assuming
  current Homebrew Ruby metadata matches the historical bottle's exact recipe.
- Some installed receipt build-time dependency versions differ from the dylibs
  actually shipped. Both OpenSSL 3.5.9 and 4.1.0 sources are included. This is not
  an assertion of bit-identical bottle reconstruction.
- The source payload contains committed project source, six full offline Git
  bundles (Chiaki plus required submodules), raw upstream archives/patches,
  original recipes/notices, build/relink/modification instructions, manifests and
  hashes. It never copies the working tree, live credential files or Git config.

## Assembly and integrity boundaries

`scripts/prepare-release.py` has explicit fetch, offline assemble and verify
commands. It has no publish/install command. It requires exact app build/lock,
original recipe bytes and all runtime library versions, and compares actual
Mach-O files against the expected app/engine/dylib source coverage. Missing code,
extra frameworks, incomplete patches, unknown macOS resources, path aliases,
external links, unsupported patch directives, bad hashes and unsafe exports fail
closed. Existing output directories are not overwritten.

Only HTTPS pinned input is fetched, with credential-bearing/non-HTTPS URLs and
redirect downgrades rejected, a per-file download bound and verification before
atomic cache insertion. Archive inputs are copied, not executed by assembly.
Project export rejects symlinks/special entries; Git bundles contain objects,
not user Git configuration/credentials. Source checksums verify integrity, not
publisher identity or legal compliance. Linux-only OpenSSL Perl test resources
are not required by the supported macOS build and are explicitly excluded.

## Fresh verification

- TDD: source-preparation tests initially failed for missing implementation;
  later negative regressions exposed canonical-path aliasing, ignored patch
  directives, hidden/missing Mach-O source coverage and unhandled resources.
  Frozen-tree binding also rejects changed/extra non-code resources and file
  permission changes. All **12** release-specific tests now pass.
- Bootstrap: **39/39 pass**, including the final release tests.
- Full native suite: **13/13 targets pass**, 184.59 s, including transport
  (175.40 s). No current-transport pass is inferred from the old build-4 run.
- App unit runner: **92/92 pass**. This is not interactive accessibility evidence.
- Accepted bundle: **15 Mach-O checks pass**, arm64/macOS minimum, strict ad-hoc
  signatures and complete recursive runtime linkage. It has 13 dylibs.
- FFmpeg policy: **0 codecs, 2 parsers, 1 Matroska muxer, 0 demuxers/protocols**;
  actual runtime reports **LGPL version 2.1 or later**.
- All 12 libssh2 patches apply in order to the pinned 1.11.1 source using
  Homebrew's actual `patch -g 0 -f -p1` semantics. A first `git apply --check`
  experiment rejected a hunk; no patch bytes were changed to force acceptance.
- Prototype source round-trip: 259 hashed payload files, six Git repositories
  restored from local bundles, pinned FFmpeg rebuilt, patched core and Release
  app built successfully in **145.24 s**. Git allowed only local file transport
  and the FFmpeg source archive was seeded. Source payload reverified afterward.
  **Uses installed Homebrew libraries/build tools**: not an all-dependency source
  rebuild, an OS-network sandbox, a bit-identical reproduction or clean-machine
  launch. No PS5 connection or app UI was launched by that reconstruction test.
- Logs/results: `build/release-preparation/final-*.log`,
  `libssh2-patches.log`, `rebuild-native-1.log`, `rebuild-app-1.log`,
  `rebuild-verification-1.json` and `component-inventory.json`.

## Artifacts

The app ZIP candidate is
`build/release-preparation/FreePS5CaptureCard-0.1.0-macos-arm64.zip`,
8,545,506 bytes, SHA-256
`dc2e44ec779d7029be646d785064bed0df40b024c53b8b7dcc4b24cb816150b1`.
It archives the existing accepted app without changing its files.
Final source: `build/release-preparation/FreePS5CaptureCard-0.1.0-source.tar.gz`,
**311,156,992 bytes**, SHA-256
`d7bf8fc04245bc621b4087ce9f94d0f0db8a9e98b1b5da7f63a2e0d61d721bf2`.
It has **260** verified payload files, including the final checker, modification/
relink guide and a frozen app inventory. Its project/upstream/notice payloads
are byte-identical to those in the successful source-reconstruction experiment.
All six final bundles were separately restored again at their pinned revisions;
the final archive and app ZIP both pass fresh round-trip extraction/integrity
checks, including frozen app file/link/mode identity and the 15 binary checks.
This final assembly/round-trip took 23.6 s; the native/app build was not repeated
for documentation/checker/inventory-only payload changes.

Prepared alongside these archives:

- `FreePS5CaptureCard-0.1.0-manifest.json`: 44,113 bytes, **19** source component
  records, exact archive/app hashes, source/license filename inventories,
  verification scope and open gates. SHA-256
  `e7c8a0364a525214b265d0fa7628da5784fbefa1a3870fb5525bdaa3f5ad9043`.
- `SHA256SUMS`: hashes of both archives and that manifest. Integrity hashes are
  not publisher signatures or an independent attestation.

All are under `build/release-preparation/`. Prototype `source-verification-1.tar.gz`
and source-build8-1/roundtrip-1 trees are verification material, not release assets.
No release assets have been uploaded; the remote draft is still unpublished.

## Independent review requested — not completed

Review the new release tooling/inputs/tests against `1151999..main`, and the
accepted app source/locked patches at the frozen runtime revision. Concrete questions:

1. Does the supplied source, patch/resource inventory, modification notice and
   rebuild/relink procedure fulfill the applicable covered-work obligations for
   these **actual** static/dynamic components? Resolve any notice/data-file or
   historical-bottle provenance gaps; recipes/URLs alone are not a source offer.
2. Review the actual AGPL/GPL/LGPL/OpenSSL-exception/Apache combination and chosen
   library-license alternatives. Neither this inventory nor a green runtime
   checker is legal compatibility approval or a patent/distribution certification.
3. Review all first-party app/native code, credential boundaries, protocol policy,
   bounds/lifetime fixes and source-preparation filesystem/network/export behavior.
   The author cannot independently approve their own work.
4. Confirm actual clean-account/machine launch without development paths and the
   outstanding calibrated PS5/OBS HDR/sync/soak/UI acceptance. Basic user-reported
   operation and accepted icons do not establish these stronger results.

Do not publish or upload app assets while the required review/manual gates remain
open. No Applications install, notarization, security bypass or app-behavior
change is authorized implicitly by this preparation. The unrelated local deletion
of `docs/plans/file-credential-storage.md` stays uncommitted.
