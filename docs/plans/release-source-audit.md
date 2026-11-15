# Release source, README and upstream-license audit

## Goal and scope

User approved the full release-focused source review and requests a release README,
Chiaki-compatible open-source licensing (worded "opengl") and upstream credit.
OpenGL is not introduced: this is a no-decoder capture app and current project
licensing is already AGPL-3.0-only. Inspect/preserve actual upstream AGPL/OpenSSL
exception notices rather than invent a GPL/MIT relicensing. Escalate any actual
license choice before changing existing license grants. No publication authorized.

Review baseline is `365b80e` on main. Preserve the unrelated unstaged deletion
`docs/plans/file-credential-storage.md`. First-party source inventory: 24 App text
files (1,882 lines), 33 Native files (2,665), three CMake helpers (62), two locked
patches (707), eight scripts (397), plus build/config/CI and supporting tests.
Vendored Chiaki and transitive dependencies are a separately bounded integration/
licensing surface, not a claim of auditing every line of every upstream project.

## Progress

- [x] Inventory first-party code, patches, tests, build and packaging.
- [x] Inspect current root/upstream license declarations and notices.
- [x] Read first-party production code and locked patches by subsystem.
- [x] Review test coverage, packaging/runtime closure and source obligations.
- [x] Record prioritized findings and explicit per-file coverage in
  `docs/reviews/release-source-audit.md` (author review, not independent review).
- [x] Reproduce/fix the in-scope curl-notice defect with narrow red/green tests;
  retain unresolved source-delivery and independent/device gates explicitly.
- [x] Improve README, explicit Chiaki/chiaki-ng credits and licensing/source docs.
- [x] Verify scoped changes and assemble a separately named developer audit bundle.
- [x] Create the scoped local checkpoint commit (no push/publication); includes
  this plan, audit report, notices/docs and the curl packaging regression/fix.

## Review passes

1. Security/data ownership: auth redirect/state, transient secrets, private file
   bounds/permissions/atomicity, local TCP limits and untrusted packet parsing.
2. Correctness/lifecycle: native callbacks, locks, stop/join, cancellation,
   asynchronous UI state, media/config generations, timestamp and queue recovery.
3. Release/content: complete dependency/license inventory, exact upstream notices,
   patched-source provenance, source availability, binary build metadata, README
   support/limitations and honest release evidence.

Run these read-only passes in the controller thread; no subagents were requested.
Findings need file/line evidence and reproduction where practical. Track coverage
separately from passing tests. No complete-codebase or independent-review claim
without the corresponding evidence. Do not read real credential files or enable
raw protocol logs. Never modify `/Users/sacherif/Projects/chiaki-ng`.

## Verification strategy

Use existing dependency-free Bootstrap tests, direct app unit tests, specific native
targets and the smallest integration cases for findings. Add failing regressions
before behavioral changes. Test package/source manifests without exposing secrets.
Time expensive stages, preserve Debug/Release caches, and parallelize independent
commands (not shared writes). Run broader checks once at a stable checkpoint.
Existing hands-on sync/HDR/latency/soak/clean-machine gates remain at final handoff.

## Surprises & Discoveries

- Root LICENSE is already AGPLv3, with AGPL-3.0-only stated in notices. The pinned
  core's source SPDX identifier is LicenseRef-AGPL-3.0-only-OpenSSL, with its
  exception text preserved in LICENSES. Existing notices credit account-ID code
  adaptations but README lacks prominent upstream credit.
- Developer packaging includes notices and formula recipes. That alone does not
  establish compliance with corresponding-source obligations for distributed
  patched Chiaki/FFmpeg and transitive linked libraries; review actual manifests.
- Static curl was linked into CaptureEngine but omitted from both notice-copy and
  validation lists. Two focused tests failed before adding curl and passed after.
  Actual packaged curl `COPYING` matches the pinned source bytes.
- The full Debug CTest run includes upstream `unit` plus nine capture targets:
  10/10 passed in 187.30 s; transport integration alone used 175.07 s. The warmed
  Release developer package completed in 9.69 s with 28 Mach-O checks passing.

## Decision Log

- Preserve existing license grants and upstream exceptions; no OpenGL graphics work.
- Scope production review to all first-party source/patches/build entry points;
  explicitly list the vendor integration surfaces and limits.
- Do not mark independent review or public redistribution cleared by self-review.

## Outcomes & Retrospective

Author audit and exact coverage are in `docs/reviews/release-source-audit.md`.
The report enumerates all 68 App/Native/CMake-helper/script text files plus both
patches and root build/config/CI inputs; supporting test review and vendor scope
limits are explicit. No new production app/native code bug was established.

README now credits original Chiaki/chiaki-ng and explains retained AGPL terms;
`docs/source-distribution.md` inventories actual candidate dependencies and the
unfulfilled corresponding-source work. No license grant was changed.

Fresh evidence: 13 Bootstrap tests passed (including two red/green regressions),
10 CTest targets passed, and `build/package-source-audit/Free PS5 Capture Card.app`
passed recursive closure/signature checks. This remains 0.1.0 build 2, not RC2.
Original RC1 ZIP SHA-256 remains unchanged. All local links in the six release/
audit documents resolve; whitespace checks pass. The generic guide audit reports
only an unrelated missing `docs/skills/README.md` (this app has no managed skill
guide family); do not add an irrelevant cookbook docs tree.

Do not redistribute original RC1 unchanged: it lacks curl's notice. Public binary
release remains blocked on exact corresponding-source assembly/delivery,
independent review and the outstanding hands-on/clean-machine evidence. No
publishing, install, notarization or real credential access occurred.
