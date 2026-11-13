# Review request: native PSN identity setup

Scope: `App/AccountModel.swift`, `App/Models/Account.swift`,
`App/Services/{PSNAuthService,CredentialStore}.swift`, account UI/startup/quit
integration, and the three new app test classes. No console pairing is included.

Expected behavior: exact trusted HTTPS redirect and fresh expiring state;
single-use authorization codes; properly encoded forms; exact uint64 identity;
no auth-secret logging/plaintext persistence; no HTTP redirects; bounded ephemeral
HTTP responses; generation-safe cancellation; no stale identity writes; only the
derived identity stored in the app's non-synchronizing Keychain item.

Evidence: observed red auth/store/model tests; final native/core/transport tests
pass 3/3 CTest targets; 25/25 XCTest and 4/4 bootstrap cases pass. Debug/Release
build and signature verification pass. Unit tests inject fake HTTP/Keychain
boundaries, so do not claim real Sony or Keychain access-policy validation.

Review focus: callback/async cancellation races, code/state consumption, response
redaction, token-bearing URL handling, Keychain record validation/access policy,
test-host isolation, and the actual provider's state behavior. Current controller
inspection found no blocking issue for a private development test; no independent
security review has been performed. Manual Gate B and review remain release gates.
