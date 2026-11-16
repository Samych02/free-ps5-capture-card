# Execution Plans

Active release preparation: `docs/plans/first-downloadable-release.md`. User chose
a downloadable build-8 app plus complete matching source, not source-only preview.
Keep behavior fixed and release in draft while source/review/manual gates are open.

Current follow-up: `docs/plans/capture-only-negotiation.md`. User chose a new
developer candidate with capture-only policy enabled; PS5 compatibility is pending.

Current remediation: `docs/plans/offline-audit-fixes.md` (four approved offline
audit fixes; controller/microphone negotiation remains unchanged).

Active follow-up: `docs/plans/minimal-ffmpeg-portability.md` (minimal FFmpeg and
portable foundations; current build remains Apple Silicon/macOS 26 only).

Use an Execution Plan for multi-hour work spanning implementation and validation.
Plans live in `docs/plans/` and must be restartable without chat history. Include
Progress, Surprises & Discoveries, Decision Log, and Outcomes & Retrospective.
Record current files, commands, observed test results, remaining gaps and the next
concrete step. Update the active plan at checkpoints and on design discoveries.

Current continuation: `docs/plans/finish-live-capture.md`, including the private
file credential follow-up that supersedes the older Keychain-only policy.
It implements the
remaining parts of Tasks 6–10 from `docs/plans/free-ps5-capture-card.md`.
