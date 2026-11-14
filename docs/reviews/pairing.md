# Review request: local PS5 discovery and pairing

Historical milestone request. Its Keychain backend was subsequently replaced
by the approved [local-file design](../architecture.md).
Native pairing/identity/lifetime requirements still apply; current storage review
is requested in [the handoff](../review-handoff.md).

Scope: Task 4 changes since `bc30ee7`, specifically native Discovery/Pairing and
the C interface, Swift NativeConsoleService/PairingModel/PairedConsole, atomic
Keychain record migration, pairing/account UI and startup/quit integration, tests
and guides. No media path, embedded browser, or reference Chiaki checkout edits.

Expected behavior: local PS5-only registration, eight ASCII pairing digits,
borrowed callback copies, no raw library logging or AP/Wi-Fi-secret persistence,
stop/join ownership, generation-safe cancellation, account/address/MAC/target
binding, durable Keychain save before success, and preservation of the old record
on failed/cancelled replacement. The normal browser login stays unchanged.

Evidence: observed failing native snapshot/copy/validation/lifecycle tests and
Swift model/store/bridge tests before implementation. Added a failing regression
for shutdown returning during an outstanding native join and fixed it by tracking
cleanup tasks. Explicit discovery send permission errors and missing key payloads
also reproduced in failing tests before their fixes. Full native/app suites pass:
4 CTest targets (118 upstream, 3 interface, 5 pairing/discovery cases, synthetic
transport), 41 XCTest cases, 4 bootstrap cases. Debug/Release builds pass.
Ad-hoc signature verification passes for both configurations; the native setup
target also passes 10 repeated runs with ASan/UBSan in a separate build directory.

Controller inspection: fixed the shutdown race; confirmed old pairing survives
cancel/failure/denied writes and stale callbacks cannot write under a new account.
Review especially callback reentrancy, cancellation-vs-completion, concurrent
shutdown, record migration/access policy, and secret retention/redaction. Native
API callers must serialize destroy with other handle access and never destroy
from callbacks. Swift holds each context through its joined teardown.

Known limits: IPv4 discovery only; scoped IPv6/hostnames are not accepted for
manual pairing. Explicit EACCES/EPERM from init or the pinned core's discovery
send-error logger is recognized without exposing raw logs. Silent suppression
or ambiguous host-unreachable cannot be conclusively classified as TCC denial.
Real Gate C, independent code/security review, and older-OS dependency/bundle
validation remain pending. This is a private development checkpoint, not release
approval or evidence of live PS5 A/V capture.
