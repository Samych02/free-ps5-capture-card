# Offline audit remediation — author verification

## Verdict and scope

Four approved offline findings are repaired and automated checks pass. This is
author verification, not independent approval, a live exploit demonstration or a
public-release clearance. The user explicitly deferred any decision about
controller/microphone **negotiation** until real-PS5 testing is available. That
code remains unchanged; no app-side controller/microphone features were added.

## Findings and repairs

| Finding | Repair | Evidence / limits |
| --- | --- | --- |
| P1 control-frame length overflow | Locked patch 0003 checks payload against buffer capacity before addition/decryption. The same checked size_t length controls dispatch, subtraction and memmove. READY consumes at least eight bytes. | Boundary, all eight wrapping lengths, unaligned/fragmented/coalesced/progress cases and 100,000 deterministic headers pass under ASan/UBSan. No live attack or whole-network fuzzing claim. |
| P2 repeated identical PIN rejection | C API v3 carries typed process-unique request IDs; Swift clears submission on a new request, not changed error text. | Real AppModel regression failed before the fix and passes after it. Native tests cover repeated requests and replacement sessions; bridge preserves full UInt64 IDs. No real-console PIN test performed. |
| P2 credential ACL privacy gap | Open directory/lock/record descriptors reject all granting ACL entries, including inherited/inherit-only/owner grants; deny-only/empty ACLs remain allowed. Inspection errors fail closed. No ACL is silently altered. | Granting/inherited ACL regressions failed before the fix and pass after. Inherited unsafe directories prevent lock/record creation. Synthetic files only; no real credentials or actual cross-user exposure inspected. |
| P2 CI prerequisite provisioning | Workflow installs the documented Homebrew prerequisites before bootstrap. | Ordering and all lockfile formula names are checked offline. A fresh hosted runner was not executed; do not claim CI passed remotely. |

The audit's first finding needs precise impact language: the old expression can
wrap its consumed-byte count to zero while the memmove source pointer advances
four GiB on this target. Unsafe memory access/crash is a credible consequence;
an infinite native-session loop was not demonstrated. Repair addresses both the
invalid size and inconsistent pointer arithmetic before those paths are entered.

The ACL policy is intentionally conservative: even an explicit owner-only granting
entry is rejected rather than resolving arbitrary principal/access combinations.
Darwin reports no extended ACL as nil/ENOENT; this normal absence is allowed.
Populated ACLs are validated before checked FIRST/NEXT iteration. Other failures
remain storage errors, never a fallback to another copy/store.

## Cached-source build safety

FetchContent may skip its patch command for an already-populated source checkout.
The dependency wrapper now revalidates/applies the complete lock every configure.
A regression failed before this guard. A semantic check deliberately reversed
only our patch 0003 in the Release fetched source; an ordinary build restored it
and succeeded in 2.37 s (`build/audit-cached-patch-refresh.log`). Both fetched
configuration trees are patched; source fixes are not stranded in ignored caches.
All three digest-locked patches also apply to a pristine pinned upstream archive.

## Fresh verification

- `python3 -m unittest discover -s Tests/Bootstrap -v`: all Bootstrap checks,
  including CI ordering and cached-patch guards: **27/27 pass**.
- `bash scripts/test-app-unit-direct.sh`: **92/92 tests pass**, 8.94 s end-to-end.
  Logs: `build/audit-pin-red.log`, `build/audit-acl-red.log`,
  `build/audit-app-green.log`. This is the explicit direct unit runner, including
  off-screen layouts—not interactive UI/accessibility or real-device evidence.
- `ctest --test-dir build/native/Debug --no-tests=error --output-on-failure`:
  **12/12 targets pass**, 187.59 s; transport accounts for 173.82 s.
- Focused parser `clang++ -fsanitize=address,undefined` regression passes with all
  boundary/progress/fuzz cases. This instruments the same internal framing helper
  used by the patched core; it is not a comprehensive dependency sanitizer audit.
- Fresh Release developer app: `build/package-offline-audit/Free PS5 Capture Card.app`,
  **0.1.0 build 3**, matching C API v3. Package built/checked in 12.46 s: **15 Mach-O
  files**, arm64/minimum OS/recursive closure/strict signatures; **13 dylibs**.
  Packaged engine contains the new control-frame guard and three-patch provenance.
- ZIP: `build/FreePS5CaptureCard-0.1.0-dev3-offline-audit-arm64.zip`, **8,543,748 bytes**,
  CRC passes, SHA-256 `5317744a48a82290cd70f7b99e576b624afd4dd2cba41cea48a14d93ff262327`.
  Original RC1 and minimal-portable ZIP hashes are unchanged. Source/library
  architecture remains Apple Silicon/macOS 26 only; minimal FFmpeg is retained.
- Exact upstream feature-negotiation function is byte-for-byte unchanged. New
  patch does not touch streamconnection.c or controller/microphone signaling.

## Author review

Read-only controller passes covered bounds/progress and patch provenance;
PIN event identity/lifecycle/ABI consistency; descriptor ACL checks, absence/error
semantics and non-mutation; CI ordering and honest test/platform/license claims.
No additional scoped blocker was established after the regressions passed.

## Deferred / missing evidence

- Controller/microphone negotiation removal decision, explicitly deferred by user.
- Actual PS5/OBS PIN, game transitions, HDR/sync/latency/soak and clean-machine tests.
- Core receiver counter-reset filtering integration: existing direct pipeline
  reset fixtures do not establish that reset packets reach it through Chiaki.
- Packaging without test builds, zero-copy packets and event-driven wakeups are
  measurement candidates, not proven bottlenecks or part of this fix scope.
- Hosted CI execution, independent review and complete matching-source delivery.

See [the execution plan](../plans/offline-audit-fixes.md) and
[release checklist](../release-checklist.md). No real credentials were read,
no old Keychain entries touched and nothing published/installed/notarized.
