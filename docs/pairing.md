# Pair your PS5 — manual Gate C

The app now discovers and pairs a local PS5 independently of Chiaki settings.
Live capture is the next milestone; **Start Capture remains disabled**.

## Before you start

- Connect the Mac and PS5 to the same local network. Keep the PS5 switched on.
- Complete [PSN sign-in](account-setup.md) for the account used on that PS5.
- On PS5, enable **Settings → System → Remote Play → Enable Remote Play**.
- Allow the app's local-network access if macOS asks. If previously denied,
  check **System Settings → Privacy & Security → Local Network**.

## Pair

1. Open `build/xcode/Build/Products/Release/FreePS5CaptureCard.app` after building
   with `scripts/build.sh --config Release`.
2. Click **Pair PS5…**. Select a discovered console, or enter its numeric IP
   address manually. Find it in **PS5 Settings → Network → Connection Status →
   View Connection Status**. Do not include `http://`, brackets, or a port.
   Discovery currently broadcasts over IPv4; manual entry also accepts unscoped
   numeric IPv6. Scoped link-local IPv6 addresses/hostnames are not supported.
3. On PS5 choose **Settings → System → Remote Play → Link Device**. Enter its
   **8-digit code only in the app**, preserving leading zeroes. This is not the
   4-digit console login passcode. Click **Pair PS5**.
4. Expect the sheet to close and the main window to show the PS5 name/IP and
   **Saved securely**. Quit and reopen; the same pairing should restore without
   contacting Sony or importing any Chiaki settings.

Pairing success means the registration response passed validation and its keys
were saved to Keychain. It does not yet prove a live Remote Play session works.

## Failure and replacement checks

- Try an invalid/expired code: expect a sanitized error, not success. Open
  **Link Device** again for a fresh code. Check console power, Remote Play,
  firewall, network isolation, account, and the numeric IP if pairing fails.
- An explicit OS permission failure is shown separately from an empty discovery
  list. macOS can also silently suppress discovery or report only unreachable;
  an empty list alone cannot establish permission denial. Check Local Network
  settings and try manual IP entry (which needs no discovery result).
- **Replace Pairing…** keeps the old saved pairing until the replacement is
  successful and durably saved. Cancellation, failure, mismatched discovered
  console identity, or denied Keychain access must leave the old pairing intact.
- **Forget PS5…** removes console keys, not the account. **Forget Account…**
  removes both. Signing in with a different account also discards old console
  keys atomically with the new identity. A failed account replacement keeps both.
- Discovery runs only while the pairing sheet is open. Cancel/close stops it;
  Quit waits asynchronously for native registration/discovery workers to join.

The single local, non-synchronizing Keychain record contains account identity,
console MAC/target/name/address, and the required Remote Play registration and
session keys. No pairing code, login passcode, OAuth token, AP/Wi-Fi credentials,
or imported settings are saved. The existing account-only record is read without
losing identity, then migrated atomically on the next successful write.

## Report safely

Tell us whether discovery, manual IP, pairing, and quit/relaunch worked. On
failure share **only the app's error text**, never pairing codes, keys, redirect
URLs, account-ID bytes, or Keychain contents. Automated tests use fake library,
network, and Keychain boundaries. The user subsequently confirmed real pairing
and persistence after reopening. Discovery versus manual-IP coverage, failure
paths, and broader macOS permission/Keychain behavior remain manual checks.
TV/controller/account restrictions and
streaming with saved keys will be checked with live capture, not bypassed.
