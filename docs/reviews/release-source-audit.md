# Release source audit — author review

Historical baseline report: later minimal-FFmpeg/platform-boundary changes are
tracked separately in [their execution plan](../plans/minimal-ffmpeg-portability.md)
and [portability guide](../portability.md). File paths and dependency counts below
describe this audit's baseline, not a new full audit of subsequent changes.

## Verdict

**Needs Changes before public release.** First-party production-source reading,
locked-patch/integration review and build/packaging review are complete for baseline
`365b80e` plus the scoped changes described below. This is an **author audit in one
controller thread**, not independent code/security/license review, a penetration
test, a vulnerability scan or legal certification. No subagents were used.

The review found a reproducible missing curl-notice defect (fixed), incomplete
corresponding-source delivery (open release blocker), and incomplete README build
prerequisites (fixed). No additional reproducible production-code defect was
established by these passes. That statement is not proof of defect absence or
device readiness.

## Prioritized findings

### P1 — [content-engineer, quality-engineer] Corresponding source is not assembled

**Open, public-binary release blocker.** `scripts/package-dev-app.sh:22–44`
copies notices, the dependency lock and selected upstream license files;
`scripts/bundle_runtime.py:95–105` copies formula licenses/recipes and runtime
paths, not corresponding source. `Tests/Support/check_bundle.py:16–19,49–51`
checks selected directories and signatures but cannot establish source compliance.

The actual bundle includes modified AGPL Chiaki, static submodules and GPL-enabled
FFmpeg with x264/x265 and LGPL/permissive transitive libraries. `ffmpeg -L`
reports GPL-3.0-or-later, `--enable-gpl --enable-version3`, with no
`--enable-nonfree` flag shown. Treating this as an LGPL-only FFmpeg package or
assuming generic upstream links fulfill source obligations would be incorrect.

**Action:** follow [source-distribution.md](../source-distribution.md): freeze
exact versions/configuration, assemble matching covered sources and build/install
materials, verify the source-delivery method and obtain independent license
review. Notice/README improvements do not close this gate. No public binary has
been authorized or published.

### P2 — [senior-engineer, content-engineer] Static curl notice omitted

**Fixed for new bundles; original RC1 remains affected.** Baseline
`scripts/package-dev-app.sh:34` and `Tests/Support/check_bundle.py:17` enumerate
Chiaki/gf-complete/Jerasure/nanopb but not curl. Release `build.ninja` links
`_deps/chiaki-build/third-party/curl/lib/libcurl.a` into CaptureEngine. RC1's
`ProjectNotices/EmbeddedSources` has no curl directory. Upstream
`third-party/curl/COPYING` requires its copyright and permission notice in copies.

**Repair:** add curl to both lists; preserve the complete upstream notice bytes.
`Tests/Bootstrap/test_package_notices.py` exercises the actual embedded copying
program against a synthetic source tree and tests rejection when curl is absent.
Both tests failed on baseline for the intended missing behavior and passed after
the two-line fix. A fresh developer package verifies actual-source notice copying.
Do not distribute original RC1 unchanged or describe it as fully notice-complete.

### P2 — [quality-engineer, security-engineer] Device and independent evidence missing

**Open acceptance gate, not a newly proven code bug.** The
[release checklist](../release-checklist.md) still lacks independent review,
interactive accessibility/current-candidate setup/stop/PIN evidence, calibrated
PS5/OBS A/V and HDR verification, latency analysis, long-run fault/transition
evidence and clean-machine launch. Automated fixtures cannot replace these.

Earlier user confirmation of pairing, synthetic OBS feeds and successful HDR
pause recovery is retained as that specific evidence only. In particular,
source retries 0 / OBS resets 2 is not uninterrupted OBS output or calibrated
sync/HDR acceptance. Hands-on gates remain deferred to final handoff as requested.

### P3 — [content-engineer] README omitted prerequisite installation

**Fixed.** Baseline `README.md:58–61` directs readers from Xcode/Homebrew straight
to bootstrap; `scripts/bootstrap.sh:5–10` requires additional tools/formulae and
does not install them. README now lists the actual Homebrew prerequisites and
explains bootstrap's role. It also corrects “OBS output” to the current
“OBS connection” section name and adds explicit upstream attribution/license
and source-delivery limitations.

## Review method and observations

Post-implementation scope: production code, workflow and docs. Separate read-only
controller passes used these lenses, then merged duplicate findings:

- **senior-engineer:** native/Swift ownership, callback teardown, cancellation,
  lifecycle/lock order, parser/queue/mux boundaries and minimal repairs.
- **security-engineer:** OAuth origin/state/expiry/replay/redirect checks, ephemeral
  bounded responses, sanitized errors, file modes/type/owner/link validation,
  atomic expected-value updates, local transport and untrusted media bounds.
- **quality-engineer:** tests mapped to boundary/failure paths, red/green evidence,
  distinction between deterministic fixtures and actual-device acceptance.
- **content-engineer:** source provenance, upstream credit, license/build claims,
  packaging notices and evidence-backed README/release instructions.

Security/lifecycle observations are bounded review conclusions, not guarantees:

- Credential-file operations reject unsafe modes, symlinks/nonregular files and
  stale updates; file data is bounded to 64 KiB. Locking is nonblocking and updates
  use same-directory temporary-file/rename operations. No real credentials were
  read. No Keychain migration/fallback was introduced.
- OAuth validates a trusted Sony redirect and per-attempt state before exchange,
  consumes valid codes once, rejects HTTP redirects, bounds responses and discards
  cancelled generations. Raw network/protocol errors are not exposed to the UI.
- Swift relays copy borrowed native records and keep callback context alive through
  background joins. Setup/capture mutations use main-actor state and generation
  guards. Native sinks are registered before start; teardown joins receive threads
  before freeing callback context.
- Queue storage/admission/age and socket writes are bounded. Codec parsing opens
  no media decoder. Two tracks are required; protected media is dropped and HDR
  mismatches do not silently fall back. Source media-pause recovery retains the
  Remote Play session while refreshing output timing.
- **Accepted trust limits:** the unencrypted file is accessible to processes
  running as the same macOS user. The unauthenticated IPv4 loopback media endpoint
  can be consumed or occupied by other local users/processes; loopback is not
  authorization or encryption. Do not forward the port. These limitations are
  disclosed rather than silently changing the one-source OBS architecture.
- Resource budgets constrain owned media, not whole-process RSS or every upstream
  allocation. Source timing/counter diagnostics do not measure lip-sync/latency.

## Exact production coverage

All listed text files were read at production/integration level. Paths grouped
below are separate reviewed files, not inferred coverage from passing tests.
Baseline totals: App 24 text files / 1,882 lines; Native 33 / 2,665; CMake helpers
3 / 62; locked patches 2 / 707; scripts 8 / 397. App text includes two asset
catalog JSON files; PNGs are artwork, not code. Original icon generator and
catalog dimensions were reviewed; this is not third-party artwork clearance.

### App — full production reading

| Area | Files | Main checks |
| --- | --- | --- |
| State/lifecycle | `App/AccountModel.swift`, `App/PairingModel.swift`, `App/AppModel.swift`, `App/AppDelegate.swift`, `App/FreePS5CaptureCardApp.swift` | cancellation/generations, restore, start/stop/PIN, quit/reopen, release test guards |
| Native bridge | `App/EngineClient.swift`, `App/Services/NativeConsoleService.swift` | borrowed copies, identity, relay lifetime, off-main join, shutdown overlap |
| Auth/storage | `App/Services/PSNAuthService.swift`, `App/Services/PrivateCredentialFile.swift`, `App/Services/CredentialStore.swift` | trust boundaries, transient secrets, bounded owner-only atomic persistence |
| Models | `App/Models/Account.swift`, `App/Models/PairedConsole.swift`, `App/Models/StreamOptions.swift`, `App/Models/CapturePresentation.swift` | validated IDs/keys/IPs/options, signed metrics, current-vs-history status |
| Views | `App/Views/MainView.swift`, `App/Views/AccountConsoleView.swift`, `App/Views/PairingSheet.swift`, `App/Views/ChangeAddressSheet.swift`, `App/Views/LoginPasscodeSheet.swift`, `App/Views/StreamSettingsView.swift`, `App/Views/OutputMetricsView.swift`, `App/Views/UtilityComponents.swift` | editable-state guards, masked/transient codes, stale sheets, port validity, status disclosure |
| Assets | `App/Assets.xcassets/Contents.json`, `App/Assets.xcassets/AppIcon.appiconset/Contents.json` | source-generated catalog entries and release icon selection |

### Native — full production reading

| Area | Files | Main checks |
| --- | --- | --- |
| Public bridge/build | `Native/include/CaptureEngine.h`, `Native/module.modulemap`, `Native/CMakeLists.txt` | API v2 value sizes/visibility, callback contract, link boundaries |
| Engine/setup | `Native/src/Engine.cpp`, `Native/src/EngineInternal.h`, `Native/src/Pairing.cpp`, `Native/src/Discovery.cpp`, `Native/src/SetupInternal.h` | validation, worker cancellation/join, connection/key wiping, bounded copied hosts, permission reporting |
| Session/control | `Native/src/Session.cpp`, `Native/src/Session.h`, `Native/src/SessionInternal.h`, `Native/src/EncodedMedia.cpp`, `Native/src/EncodedMedia.h` | initialization, sink ordering, callback ownership, exception barriers and IDR lifetime |
| Media orchestration | `Native/src/Pipeline.cpp`, `Native/src/Pipeline.h`, `Native/src/PacketQueue.cpp`, `Native/src/PacketQueue.h` | locks, generations, bounded owned buffers/leases, output epochs, two-track recovery and restriction handling |
| Clock/diagnostics | `Native/src/Timeline.cpp`, `Native/src/Timeline.h`, `Native/src/Diagnostics.cpp`, `Native/src/Diagnostics.h` | counter wrap/reorder/overflow, audio anchoring, retries, sanitized current/history messages |
| Format/parsers | `Native/src/AnnexB.cpp`, `Native/src/AnnexB.h`, `Native/src/AudioFormat.cpp`, `Native/src/AudioFormat.h`, `Native/src/LiveFormat.cpp`, `Native/src/LiveFormat.h`, `Native/src/SPSMetadata.cpp`, `Native/src/SPSMetadata.h` | length/bitfield bounds, random access/reorder admission, Opus duration, parameter-set/HDR negotiation |
| Transport/mux | `Native/src/LocalTCPServer.cpp`, `Native/src/LocalTCPServer.h`, `Native/src/MatroskaMuxer.cpp`, `Native/src/MatroskaMuxer.h` | loopback binding, bounded writes, cancellation/epochs, explicit two tracks, no app playback |

### Dependency patches and build/release entry points — full reading

- `patches/chiaki/0001-embeddable-build.patch`
- `patches/chiaki/0002-encoded-media-sink.patch`
- `CMakeLists.txt`, `Dependencies.lock.json`, `project.yml`, `.github/workflows/macos.yml`
- `cmake/ChiakiDependency.cmake`, `cmake/ApplyChiakiPatches.cmake`, `cmake/BundleRuntime.cmake`
- `scripts/bootstrap.sh`, `scripts/build-native.sh`, `scripts/build.sh`, `scripts/test.sh`
- `scripts/test-app-unit-direct.sh`, `scripts/generate-app-icon.swift`
- `scripts/package-dev-app.sh`, `scripts/bundle_runtime.py`
- Release validation helper `Tests/Support/check_bundle.py`

### Supporting tests and vendor limits

Supporting tests were mapped to production boundaries using test cases, native
target registration and prior validation evidence. This is **not** a claim that
every line of the test harness has a separate security audit:

- `Tests/App/`: account/auth, private-file persistence, pairing/IP/stale results,
  capture state/PIN, native bridge/options, presentation and real-view rendering.
- `Tests/Native/`: API/setup, encoded receiver/sink, timeline, queue, Annex-B/Opus,
  session/control/protected-content and diagnostics suites.
- `Tests/Support/`: synthetic fixtures, compressed equivalence, format/HDR,
  transport/fault/transition integration. Synthetic encoders/decoders are test-only.
- `Tests/UI/CaptureUITests.swift`: credential-free smoke tests; interactive run
  remains unverified on this host. Compilation/rendering is not interactive proof.
- `Tests/Bootstrap/`: policy/build/icon and newly added packaging-notice checks.

Vendor review covered pinned Chiaki license/provenance, selected CMake/link and
auth reference paths, the complete two local patches, static submodule license
texts, and the actual bundle's dynamic closure/licenses/recipes. It did **not**
audit every line of upstream Chiaki/curl/FFmpeg/OpenSSL or the OS/toolchain, scan
CVEs or prove the complete dependency set is free of vulnerabilities. No external
reference checkout was modified.

## Verification evidence

- Red: `python3 -m unittest discover -s Tests/Bootstrap -p test_package_notices.py -v`:
  two failures, missing copied curl `COPYING` and validator reaching Info.plist
  instead of rejecting missing curl notices.
- Green: `python3 -m unittest discover -s Tests/Bootstrap -v`: **13 passed**, 1.57 s.
- `ctest --test-dir build/native/Debug --no-tests=error --output-on-failure`:
  **10/10 passed**, including upstream `unit` plus nine capture targets, 187.30 s;
  transport integration accounted for 175.07 s. This uses existing Debug binaries;
  no production app/native code changed in this audit.
- `scripts/package-dev-app.sh --config Release --output build/package-source-audit`:
  **passed**, 9.69 s, 26 non-system libraries, **28 Mach-O** files checked for
  arm64/runtime closure/minimum OS/strict recursive signatures. Full log:
  `build/release-source-audit-package.log`. This is a new **developer audit bundle**,
  still 0.1.0 build 2, not RC2 or a published release. Original RC1/ZIP were not
  overwritten. Curl notice copying is covered by the regression and actual bundle.
- Earlier 88 app tests, focused sanitizers and 30 layout renders are baseline
  evidence in [validation](../validation.md), not fresh reruns here. No redundant
  UI/device acceptance claim is made for a packaging/docs-only change.
- Local Markdown links in the six release/audit documents resolve; exact coverage
  enumeration includes all 68 App/Native/CMake-helper/script text files. Actual
  curl notice bytes match pinned source and original RC1 ZIP SHA-256 is unchanged.
  `git diff --check` passes. Generic guide-audit output has no broken links; its
  unrelated missing cookbook `docs/skills/README.md` is outside this app's scope.

## Cross-Persona Tensions

Keep the release fixes small versus prevent misleading distribution claims:
resolve the concrete notice omission now, document source obligations explicitly,
and leave source delivery/independent/device gates open. Do not replace the
approved credential/OBS architecture as speculative security hardening.

## Missing Evidence

Complete matching source artifacts and tested delivery/build; independent review;
current-candidate hands-on UI/PS5/OBS/HDR/sync/latency/soak/clean-machine evidence.
See [release-checklist.md](../release-checklist.md). No legal or public-release
approval is implied by this audit or by passing developer checks.
