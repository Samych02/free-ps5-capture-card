# Review request: native framework and app shell

Scope: changes after baseline `9053ae5` on `feat/native-app-shell`, specifically
`Native/`, `App/`, the XcodeGen project, test/build scripts, and corresponding
tests/docs. No PSN, pairing, or media-streaming implementation is included.

Expected behavior:
- C-only versioned options, H265 default, valid port/quality bounds.
- Worker callbacks with borrowed context; stop is nonblocking/repeatable.
- Destroy joins before releasing context; Swift transfers final cleanup to a
  worker. Quit waits asynchronously rather than blocking the main actor.
- The app never auto-starts capture or claims the media pipeline is ready.
- Debug/Release compile without Qt/SDL/playback initialization.

Evidence: `scripts/test.sh --native --app` passes CTest 2/2 targets and XCTest
7/7 cases. The upstream runner contains 118 cases; the new interface suite has
3. Bootstrap 4/4 pass. Release build passes. Tests were observed red first.

Review focus: callback lifetime across Swift/C++, concurrent stop/teardown,
main-actor dispatch, framework embedding/search paths, and test-selector failure
behavior. Inspect whether shutdown remains correct when adding real sessions.

Known limits: Homebrew dependencies require macOS 26; dynamic dependencies are
not bundled; manual UI/accessibility inspection and full XCUITest are pending.
This is a stable development checkpoint, not independently reviewed or approved
for merge/release. No subagent/independent review has been performed.
