# Pop-out chat takeover

Source: [PR #56](https://github.com/densestjarl/mumble/pull/56),
[`codex/pop-out-chat`](https://github.com/densestjarl/mumble/tree/codex/pop-out-chat)
at `e4d1c0bac5b694854b0aa38381bb6b259b066c12`. The PR opened October 20,
and its last commit was October 25. Repeated keyboard/lifecycle repairs and build
follow-ups left the October 25 Tab/Shift+Tab review finding unresolved.

Configure → Pop Out Chat moves the existing history, input and send-file button
into one separate window. Closing it or clearing the action restores the same
widgets, preserving history, drafts and normal dock visibility. Tab and Shift+Tab
stay inside the popped-out chat, and Escape leaves it open. Geometry is remembered
for the session and notifications follow chat; persistent pop-out settings and a
wider chat redesign are out of scope.

The takeover salvages the compact implementation and its dock-ownership/Escape
tests on current fork master `bc1ef1ed40072d163a781284ea859996e775b104`.
It omits the old branch's merge/fix-up history, fixes navigation through the chat
window, keeps the inline file-password path available, and makes the new window
state private. A real MainWindow regression exercises both keyboard directions,
close/uncheck, repeated cycles, history/drafts, the send-file container and all
four combinations of normal dock visibility across minimal view and state saves.

The original branch remains intact. Delete it only after the takeover is merged.
The other 49 October stability/evidence branches already shipped through the
October 11 squash merge of [PR #40](https://github.com/densestjarl/mumble/pull/40).
Their old commit ancestry makes them appear unmerged; preserve unique evidence
before deleting those obsolete branches.

## Verification

Full Linux Debug CMake client/server/plugins/tests build passed: CMake 3.28.3,
Ninja, GCC 13.3, Qt 6.4.2, assertions and project warnings-as-errors enabled,
unity/LTO off, screen/file sharing and chat WebM on. SQLite tests were enabled;
online and MySQL/PostgreSQL service tests were not configured. Native
Windows/macOS execution remains for CI. A fresh worktree with recursive
submodules was used; only dependency source caches were reused. All 12 compiler
warnings match the existing untouched-plugin baseline exactly
([comparison](build-warning-comparison.json)).

Full `QT_QPA_PLATFORM=offscreen ctest --test-dir <build> -j 2 --output-on-failure
--no-tests=error --output-junit <results.xml>` completed in 238.98 seconds:
**43 passed, 2 baseline failures, 1 previously disabled suite** out of 46
registered suites ([log](ctest.log), [summary](ctest-summary.json)). The file-transfer
E2E suite passed in 238.52 seconds. The new ChatWindow suite and actual MainWindow
pop-out regression passed. No assertions or tests were removed, weakened or disabled.

Failures: `TestFileTransferEngine::selectedPinSurvivesCacheRemoval` aborts on the
empty-QList assertion; `TestChatWebM` fails the existing four audio-clock/EOF
checks (`audioPlaybackAdvancesClockAndResumes`, `loopsAfterEnd`,
`resumeAtClipEndRestartsImmediately`, `pauseResumeKeepsFrameSchedule`). These
reproduce in the pre-existing November 4 binaries ([engine](cached-control-TestFileTransferEngine.log),
[WebM](cached-control-TestChatWebM.log)). Their documented source revision matches
master for the affected tests/engine/player ([comparison](baseline-source-comparison.json)).
A separate full master build was not performed. OverlayTest retains its existing disabled state.

The built client ran against a disposable local server under Xvfb for 30.25 seconds.
It exchanged chat both ways while popped out, kept Tab/Shift+Tab inside chat,
survived Escape and repeated open/restore, and preserved history and its draft
through restoration and continued typing. Observer errors: zero. Both processes
remained alive during measurement; client SIGTERM afterward was intentional
cleanup ([result](runtime-result.json), [binary hashes](binary-hashes.json)).
The driver adapted `scripts/compatibility/run_interop.py`, updating its obsolete
identity-prompt expectation and focusing Xvfb windows explicitly before typing.
This checks GUI/control behavior, not physical audio/media quality.

- [Popped-out chat](chat-popped-out.png)
- [Restored chat with continued draft](chat-restored.png)
- [One-line recommendations for the other stale branches](branch-recommendations.md)

Whitespace and changed-line clang-format checks passed. The original branch,
main checkout and sibling experiments remain untouched. Nothing was merged or
deleted; the temporary takeover worktree is removed after publication.
