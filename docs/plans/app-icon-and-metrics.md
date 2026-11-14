# App icon and metrics implementation plan

**Goal:** Give the app a distinctive capture-frame icon and let users hide all
OBS output metrics without hiding essential operation or errors.

**Approved design:** Dark macOS icon with a bright blue capture-frame mark,
without third-party logos. Use a native asset catalog with all standard macOS
icon sizes. A Show metrics disclosure defaults closed and remembers its state.
Status, errors, URL, setup instructions and Start/Stop remain outside it.

**Architecture:** AppModel owns a UserDefaults-backed presentation preference.
MainView binds a DisclosureGroup to it and encloses every existing metric row.
The engine and collection of diagnostics are unchanged. A deterministic AppKit
artwork generator produces the checked-in PNG assets, requiring no new package.

**Constraints:** Stay on main; preserve unrelated plan deletion. No media,
credential or release-policy changes. UI automation has a known host limitation;
do not equate direct XCTest with GUI acceptance.

## Task 1: Remembered metrics disclosure
- Add AppModel tests for default hidden, persistence in both directions and no
  engine starts/stops caused by toggling. Run `scripts/test-app-unit-direct.sh`
  and observe the missing-property build failure before implementation.
- Add the preference and bind MainView's full metrics block to a disclosure.
- Rerun direct app tests; add UI coverage for the disclosure and persistent state.

## Task 2: Native icon
- Add dependency-free asset/catalog wiring checks under Tests/Bootstrap; run
  `python3 -m unittest discover -s Tests/Bootstrap -v` before adding assets.
- Create `scripts/generate-app-icon.swift`, the macOS AppIcon asset set and
  `ASSETCATALOG_COMPILER_APPICON_NAME` in project.yml. Use the mark in the header.
- Generate assets with `swift scripts/generate-app-icon.swift` and verify sizes,
  asset declarations and appearance at small/large sizes.

## Task 3: Verify and hand off
- Run Bootstrap tests, direct app unit tests and Debug/Release builds.
- Package with `scripts/package-dev-app.sh --config Release --output
  build/package-icon-metrics`; inspect bundled icon and strict signatures.
- Check whitespace, update this evidence and checkpoint only scoped files.
- No PS5/OBS re-test required for presentation-only changes; GUI interaction and
  Dock appearance remain manual checks if automation cannot run.

## Completion evidence
- Red app run: `build/icon-metrics-app-red.log` fails because AppModel lacks the
  new preference. Icon checks fail before the catalog/build setting exist in
  `build/icon-metrics-icon-red.log`.
- Green: all 74 direct app unit tests pass (`build/icon-metrics-app-green.log`),
  including default-hidden and persisted show/hide state with no engine calls.
  All 11 Bootstrap/policy tests pass; icon checks validate every PNG dimension.
- Debug UI tests compile (`build/icon-metrics-ui-build.log`). They cover opening
  and closing the disclosure while preserving URL/controls, but were not run:
  existing host UI automation limitation remains. Persistence has unit evidence.
- Release packaging succeeds (`build/icon-metrics-package.log`), including 28
  arm64 Mach-O validations. The packaged plist declares AppIcon and its compiled
  ICNS exists. Strict recursive signatures pass. Candidate:
  `build/package-icon-metrics/Free PS5 Capture Card.app`.
- Artwork inspected at 1024px and 32px. No media/credential behavior changed;
  no new PS5, OBS, GUI-click or Dock acceptance claim is made.
