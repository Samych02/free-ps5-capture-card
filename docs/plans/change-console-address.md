# Change PS5 IP Implementation Plan

Historical implementation plan: the subsequent
[file-storage change](../architecture.md) replaces the Keychain backend
referenced below. Change IP semantics and stale/active-capture guards are retained.

**Goal:** Update a paired PS5's saved IP without pairing again or changing keys.

**Architecture:** Add a stopped-only Change IP sheet to the existing native
console controls. AppModel gates edits while capture is active; PairingModel
validates the current account/console snapshot, and CredentialStore replaces
only the address in the atomic Keychain record. No discovery, network verification,
PSN authentication or registration happens during this operation.

**Constraints:** Work on `main` (explicit repository/user preference). Preserve
all credential bytes and the old record on invalid input, stale identity or save
failure. No user Keychain/network access in tests. No overwrite of existing
developer packages. GUI automation remains an environment-blocked gate.

**Acceptance Signals:** IPv4/IPv6 input uses existing strict validation; all
non-address console fields remain identical; persistence survives model recreation;
bad/stale/forgotten/busy/active edits do not write; Cancel performs no operation;
Release bundle passes recursive/signature checks.

## Approved design

User chose **Change IP only**, not rediscovery. Button appears for an existing
pairing. Sheet starts with the current IP, explains key preservation and where to
find the new PS5 IP, and offers Cancel/Save IP. Invalid address or running capture
disables Save. The model repeats the active-state guard at submission, including
when another window starts capture while the sheet is open. A stale console or
account snapshot must not overwrite a newer pairing. A successful save closes
the sheet; connectivity is verified only on the next capture attempt.

## Task 1: Safe model/store update

**Files:** `App/Models/PairedConsole.swift`, `App/Services/CredentialStore.swift`,
`App/PairingModel.swift`, `App/AppModel.swift`; tests in
`Tests/App/PairingTests.swift` and `Tests/App/AppModelTests.swift`.

1. Add failing tests for key preservation/relaunch, invalid/IPv6 addresses,
   denied writes, changed/forgotten account or pairing, busy pairing and active
   capture. Scaffold only the missing API with failure/no-op results.
2. Run `scripts/test-app-unit-direct.sh > build/change-ip-red.log 2>&1`;
   expect assertions demonstrating the missing successful update.
3. Add an address-copy helper, a narrow checked store operation, a PairingModel
   operation, and AppModel's stopped-only orchestration. No native engine changes.
4. Run `scripts/test-app-unit-direct.sh > build/change-ip-green.log 2>&1`;
   expect all app unit tests pass.

## Task 2: Native controls, documentation and developer package

**Files:** new `App/Views/ChangeAddressSheet.swift`; modify
`App/Views/AccountConsoleView.swift`, `App/Views/MainView.swift`, `docs/setup.md`,
`docs/pairing.md` and this progress record.

1. Keep the existing system-control utility design; expose Change IP only for
   a saved console. Pass the shared capture model to the sheet for live guards.
2. Explain no new code/login is needed and no connectivity is claimed by Save.
3. Run `scripts/test.sh --native > build/change-ip-native.log 2>&1` and
   `scripts/test-app-unit-direct.sh > build/change-ip-app.log 2>&1`; expect passes.
4. Run `scripts/package-dev-app.sh --config Release --output build/package-address-update`
   and `python3 Tests/Support/check_bundle.py --app 'build/package-address-update/Free PS5 Capture Card.app'`;
   expect build, runtime closure and signatures pass. Do not install/publish.
5. Run `git diff --check`, then commit explicit changed files on `main` as
   `feat: update paired PS5 address without re-pairing`.

## Progress

- [x] Existing store/view/model inspected; user-approved manual-only scope.
- [x] Red/green model/store tests (`build/change-ip-red.log`,
  `build/change-ip-green.log`). Added deletion-during-save regression
  (`build/change-ip-no-upsert-red.log`): address edits use update-only writes,
  never the generic record upsert path.
- [x] Native sheet and stopped-state submission guard.
- [x] Regression checks: 56/56 app tests (`build/change-ip-app.log`), 10/10 native
  targets (`build/change-ip-native.log`), and recursive/signature checks on the
  new Release bundle (`build/change-ip-package.log`).
- [x] Updated developer package:
  `build/package-address-update/Free PS5 Capture Card.app`.
- [x] Final diff/documentation review; implementation checkpoint on `main`.

Manual GUI interaction/VoiceOver and real-device capture remain unverified.
The feature tests use isolated fake Keychain data and never contact a PS5.
