# Local Credential File Implementation Plan

**Goal:** Fully remove application Keychain integration and start fresh using an
owner-only local credential file, as explicitly approved on 2026-11-14.

**Architecture:** Keep the `CredentialStore` model boundary, replace its backend
with a versioned JSON record under
`~/Library/Application Support/FreePS5CaptureCard/credentials.json`, and inject a
small file-access interface into unit tests. Store account identity plus paired
console keys only. OAuth tokens, redirects, pairing/login codes and Sony passwords
are never persisted. There is no migration helper or Keychain fallback/access.

**Security decision:** User knowingly approved unencrypted storage instead of
Keychain. A private directory (0700) and file (0600) restrict other OS users, not
processes running as the same macOS user. Existing Keychain entries are untouched.
The app will require fresh browser sign-in and PS5 pairing. It remains independent
of Chiaki's installed configuration; Qt is not added.

**Constraints:** Work/checkpoint on `main`. No real credential or user-store
access in tests. No installation/publishing. Preserve all current model semantics,
including atomic account replacement, stale IP-edit rejection, stopped-only edits,
and no file recreation if credentials were concurrently removed.

**Acceptance Signals:** New backend persists/reloads/forgets the validated record;
rejects unsupported/corrupt/oversized files; preserves old state on failures;
uses owner-only modes, no-follow regular-file checks, bounded I/O and atomic rename;
serializes cooperating writers and compares expected record bytes before mutation.
App code/imports contain no Keychain calls or migration path. UI/docs accurately
disclose unencrypted storage. All unit/native/policy and bundle checks pass.

## Task 1: Store interface and regression scaffolding

**Files:** `App/Services/CredentialStore.swift`, new
`App/Services/PrivateCredentialFile.swift`, `Tests/App/CredentialStoreTests.swift`,
`Tests/App/AccountModelTests.swift`, `Tests/App/PairingTests.swift`,
`Tests/App/AppModelTests.swift`.

1. Replace the Keychain-specific injection interface with throwing
   `read() -> Data?`, `write(_:replacing:)`, and `delete(replacing:)` operations.
   Expected bytes include nil for file absence; mismatches fail closed.
2. Rename model/store fakes to file storage and adapt existing tests without
   weakening key-preservation/IP-edit/stale-callback assertions. Replace the old
   account-only Keychain migration test with rejection of unsupported file data.
3. Add real temporary-directory tests for persisted account/pairing reload,
   record deletion, permissions, stale writes/deletion races, symlink/hardlink
   rejection, invalid modes, oversized/corrupt/schema-mismatched files and lock
   contention. These tests must never use the real default storage directory.
4. Scaffold missing implementation with errors/no-op results, then run
   `scripts/test-app-unit-direct.sh > build/file-store-red.log 2>&1`.
   Expect failures for missing persistence, not missing symbols/test dependencies.

## Task 2: Implement the private atomic file backend

1. `FileCredentialStore` decodes only the new versioned record and sanitizes all
   errors. Account+console are one file; preserve pairing only for the same account.
2. `PrivateCredentialFile` uses a private application directory, no-follow
   descriptor-relative operations and regular-file/owner/mode/link checks.
   Reject records over 64 KiB before allocation. No raw values/errors are logged.
3. Read/write/delete use a private nonblocking advisory lock. Under exclusive
   lock, compare current bytes with the caller's expected snapshot. Write a new
   exclusive 0600 temporary file, flush and rename only after successful writing;
   clean temporary files on failure. Delete only the credential file, not broad
   directories. No automatic backup copies or silent fallback stores.
4. Run `scripts/test-app-unit-direct.sh > build/file-store-green.log 2>&1`;
   require all old model regressions and new filesystem tests to pass.

## Task 3: Wire UI, docs, policy and developer build

**Files:** `App/AccountModel.swift`, `App/FreePS5CaptureCardApp.swift`,
`App/Services/PSNAuthService.swift`, `App/Models/PairedConsole.swift`,
`App/Views/{AccountConsoleView,ChangeAddressSheet}.swift`,
`Tests/Bootstrap/test_capture_policy.py`, README and current setup/security guides.

1. Default to the file store. Replace Keychain/"saved securely" text with truthful
   local-file wording, explain owner-only but unencrypted protection, and clarify
   Forget affects only this file (not historic Keychain entries).
2. Add policy tests forbidding application Keychain imports/API calls and verify
   the Release executable has no direct `SecItem*` imports. OS TLS may still use
   system Security internally; that is not app credential storage.
3. Run `scripts/test.sh --native > build/file-store-native.log 2>&1`,
   `scripts/test-app-unit-direct.sh > build/file-store-app.log 2>&1`, and
   `python3 -m unittest discover -s Tests/Bootstrap -v`; expect passes.
4. Package with `scripts/package-dev-app.sh --config Release --output build/package-file-store`;
   run `check_bundle.py` on that app, signatures and `git diff --check`.
5. Update this progress/evidence record and commit explicit files on `main` as
   `feat: replace Keychain with private local credential storage`.

## Progress

- [x] User approved file storage, weaker at-rest protection and start-fresh scope.
- [x] Prior Keychain-only policy explicitly superseded in `AGENTS.md`.
- [ ] Red/green store and filesystem tests.
- [ ] File-only app wiring, truthful UI/docs and no-Keychain policy checks.
- [ ] Regression checks, developer bundle and checkpoint commit.

## Review and deferred gates

Controller-thread plan review: scoped to persistence, no new auth/WAN features,
no migration or hidden fallback. Existing GUI automation, actual PS5/OBS media,
clean-machine and independent code/security/license review gates remain open.
