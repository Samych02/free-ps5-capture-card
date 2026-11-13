# PSN account setup — manual Gate B

This build implements account identification only. Console discovery/pairing and
live PS5 capture are not implemented yet. Synthetic transport tests are separate.

## Try the native flow

1. Build with `scripts/build.sh --config Release`. Open
   `build/xcode/Build/Products/Release/FreePS5CaptureCard.app`.
2. Click **Sign in to PSN…**. Your default browser opens Sony's HTTPS website.
   Enter passwords and MFA codes only on Sony's site, never in chat or this app.
3. After sign-in, copy the complete final Sony redirect URL from the browser.
   It contains a private authorization code. Paste it only into the app's masked
   **Final Sony redirect URL** field; click **Finish Sign-in**.
4. Expect **Saved securely**. The app retrieves the account's exact 8-byte pairing
   identity and saves that identity in this app's local, non-synchronizing Keychain
   record. OAuth access/refresh tokens and the redirect URL are not persisted.
5. Quit and reopen the app. The saved account should restore without contacting
   Sony. **Forget Account…** removes this app's identity record after confirmation.
   No console credentials exist in this milestone.

macOS may require Keychain authorization for the development app. If access is
denied or the item is unreadable, the app reports an error and does not silently
fall back to a file/preferences. Automated tests use an injected fake store and
do not validate the user's actual Keychain access policy or sign-in.

## Security/compatibility boundaries

- The flow adapts Chiaki's compatible Remote Play client configuration; this is
  unofficial third-party integration, not a newly registered Sony application.
- Only the exact trusted Sony HTTPS redirect host/path is accepted. Codes/states
  must be single-valued, bound to an active sign-in, and unexpired (10 minutes).
- Each sign-in has a new state nonce. An accepted code is consumed once, even
  if the subsequent HTTP exchange fails. Start again after an error.
- Real Sony acceptance/return of `state` is a required manual compatibility
  check. If the provider does not echo it, stop and investigate; no hidden
  state-validation bypass or invented custom callback URL is present.
- Token-bearing API paths and provider response bodies are never shown in app
  error messages. The HTTP transport uses an ephemeral session with cookies/cache
  disabled, bounded responses, normal TLS validation, and no HTTP redirects.
- Cancelling/restarting/forgetting invalidates in-flight work so a stale response
  cannot overwrite the current identity. The old saved account remains if a
  replacement fails. The Cancel button also cancels the UI's network task.
- Fixed Remote Play client identifiers in source are distributed app constants,
  not the user's credentials. No Sony password is collected by the app.
- Debug XCTest hosting skips automatic Keychain restore; tests inject their own
  account stores. This guard is absent from Release, and does not fabricate a
  signed-in identity or bypass the real login flow.

## Report the result safely

Confirm sign-in, **Saved securely**, and quit/relaunch persistence. On failure,
share only the app's sanitized error text. **Do not share the final redirect URL,
authorization code, access token, account-ID bytes, or Keychain screenshots.**

Gate B remains pending until this real-browser and real-Keychain test succeeds.
Mocked protocol tests alone do not establish actual Sony API compatibility.
