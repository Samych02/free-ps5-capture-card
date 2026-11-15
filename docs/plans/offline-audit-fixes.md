# Repair offline audit findings

Living Execution Plan under `PLANS.md`; baseline `4683f52`, main. User approved
the parser, repeated-PIN, credential ACL and CI fixes. Preserve existing upstream
controller/microphone negotiation until a later real-PS5 decision. No console is
available now; no device result can be claimed. Performance ideas and receiver
counter-reset behavior are not added to this four-fix scope.

## Progress

- [x] Verify findings against source and isolated reproductions. The control length
  overflow also gives an out-of-bounds source pointer, not a proven infinite loop.
- [x] Add parser regression, repair through a new locked Chiaki patch, verify
  fragmented/coalesced/oversized frames and progress with sanitizer evidence.
- [x] Add repeated-rejection regression and typed native-to-Swift PIN request IDs.
- [x] Add granting/inherited ACL regressions and fail-closed descriptor ACL checks.
- [x] Add CI provisioning regression and explicitly install documented prerequisites.
- [x] Verify stable native/app/policy/package changes and scoped author review:
  27 Bootstrap checks, 12 native targets, 92 app tests, parser ASan/UBSan and
  15 package binary checks pass. All changed document links resolve.
- [x] Create the scoped local checkpoint on main, including the user-approved
  negotiation deferral in AGENTS.md. No publication or real-device claim.

## Design / Plan of Work

1. Add a small internal upstream framing helper and use it in the actual ctrl
   receive loop. Validate declared payload against fixed buffer capacity before
   adding eight bytes or decrypting. One checked size controls the guard, consumed
   bytes and memmove pointer; READY always consumes at least eight bytes. Leave
   all valid messages/negotiation behavior unchanged. Lock patch 0003 with SHA-256.
   Test the real helper and its receive-loop use; fuzz bounded header lengths and
   fragmented/coalesced frames offline with ASan/UBSan. No network exploit claim.
2. Add a typed, process-unique nonsecret PIN request generation on each native PIN
   request event, carry it in status through the C ABI and Swift bridge, and clear
   submission on a new request ID rather than message changes. Advance the C API
   version for the changed status layout. IDs must distinguish repeated identical
   rejections and replacement sessions even if intermediate callbacks are missed.
3. Inspect extended ACLs on already-open owner/mode/type/link-validated directory,
   lock and credential descriptors. Fail closed on granting entries (including
   inherited/inherit-only) or inspection errors; accept empty/deny-only ACLs.
   Do not strip/repair ACLs, copy credentials or fall back elsewhere. Root directory
   checks occur before lock/temp creation, so inherited grants cannot leak a new
   credential record. Test synthetic files only; real credentials remain unread.
4. Provision all documented Homebrew prerequisites before bootstrap in disposable
   CI. Bootstrap stays a checking/isolated-Python command, not a global installer.
   Test workflow ordering and lockfile prerequisite coverage without claiming an
   actual hosted-runner run. Preserve local machine permissions/settings.

## Verification and Acceptance

Use narrow red/green tests per finding; reuse warmed Debug/Release and FFmpeg
caches. Run dependency-free Bootstrap tests, selected native suites, focused
parser sanitizers and direct app tests while iterating. Full native transport and
fresh separately named developer package once at the stable checkpoint. Record
counts/timings and exact commands. No live/interactive/clean-runner claim without
that evidence. Scope review remains author review, not independent approval.

## Idempotence / Safety

Never touch `/Users/sacherif/Projects/chiaki-ng`. Track all dependency fixes in
locked patches, prove clean application to pinned source and refresh both fetched
configuration trees via the patch helper (FetchContent may not rerun it for an
already-populated source). Preserve unrelated deletion of
`docs/plans/file-credential-storage.md`, earlier developer artifacts and current
AGENTS.md negotiation deferral. No publishing/install/notarization/autostart.

## Surprises & Discoveries

- Prior source triage reproduced the real AppModel's second identical rejection
  remaining submitted. Synthetic production credential-file reads accepted
  granting ACLs despite 0700/0600; no actual cross-user/real-record exposure tested.
- A payload of 0xfffffff8 wraps the old frame size to zero, while the subsequent
  pointer expression advances four GiB on this target. Reject before either path.
- Parser helper was absent before the regression. New patch 0003 uses a single
  checked size_t length in the actual loop. ASan/UBSan exercise zero/max payloads,
  all eight wrap lengths, fragmentation/coalescing and 100,000 deterministic
  headers. All three digest-locked patches apply cleanly to a pristine pinned
  upstream archive; Debug/Release fetched trees were refreshed explicitly.
- Real app regression failed on the second same-text rejection before typed IDs.
  It passes with per-request process-unique IDs propagated through C API v3 and
  Swift. Native tests verify stable IDs for unchanged prompts and distinct IDs
  for repeated requests and replacement sessions. This changes no protocol message.
- Granting/inherited ACL tests failed before the ACL fix (12 assertions), while
  the new PIN regression passed. The first ACL implementation rejected clean
  files: isolated probes showed Darwin returns nil/ENOENT for no ACL, not an
  allocated empty ACL. Only that documented empty case is accepted; other
  inspection errors fail closed. Valid populated ACLs use acl_valid and checked
  entry iteration; deny-only entries remain permitted.
- CI ordering/prerequisite regression failed before adding explicit brew install
  and passes now. No hosted CI execution or local package installation claimed.
- Cached-source regression exposed that MakeAvailable alone may skip a newly
  locked patch. Configure now explicitly revalidates the lock every time. After
  reversing only patch 0003 in the Release fetched cache, a normal Release build
  restored it successfully (2.37 s). No unsafe older core silently survives a
  warmed build. See `build/audit-cached-patch-refresh.log`.
- Final app run: 92/92 pass in 8.94 s. Full native run: 12/12 targets pass in
  187.59 s (173.82 s transport). Parser ASan/UBSan checks pass. Release package
  completes in 12.46 s, 13 dylibs/15 Mach-O checks passing. Build number advanced
  to 3; C API v3 and original credential JSON version are distinct (no migration).

## Decision Log

- Keep negotiation unchanged by explicit user instruction. Offline fixes only.
- Reject granting ACLs conservatively rather than silently editing access policy;
  preserve harmless deny entries and the approved unencrypted file design.
- Use typed request IDs, not translated strings, timers or reconnect workarounds.
- No optimization refactors in this security/correctness pass.

## Outcomes & Retrospective

Four offline fixes are implemented and verified; scoped author verification is
in `docs/reviews/offline-audit-fixes.md`. Final evidence: 27 Bootstrap checks,
12 native targets, 92 app tests and 15 package binary checks pass, along with
focused parser ASan/UBSan and clean/cached patch-application checks. Whitespace
and local links pass. Changes are checkpointed locally on main. Negotiation is
unchanged; hosted CI and independent/source-distribution/device gates remain open.

Artifacts: `build/package-offline-audit/Free PS5 Capture Card.app` (0.1.0 build 3)
and `build/FreePS5CaptureCard-0.1.0-dev3-offline-audit-arm64.zip` (8,543,748 bytes;
SHA-256 `5317744a48a82290cd70f7b99e576b624afd4dd2cba41cea48a14d93ff262327`).
Both earlier ZIP hashes are unchanged. Logs: `build/audit-pin-red.log`,
`build/audit-acl-red.log`, `build/audit-app-green.log`,
`build/audit-cached-patch-refresh.log`, `build/offline-audit-package.log`.
