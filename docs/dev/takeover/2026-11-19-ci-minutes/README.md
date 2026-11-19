# CI minutes takeover — 2026-11-19

Routine source-only pull requests should run the existing platform tests without also rebuilding every preview installer. Packaging/build/dependency/resource changes, an explicit `ci:installers` request, master pushes and manual runs must retain all package, installation, regression and QR publication gates. CodeQL should perform one traced C++/Python analysis while continuing to publish both historical comparison identities. Workflow artifacts should have bounded retention without expiring independently published release downloads.

Selected: [PR #63](https://github.com/densestjarl/mumble/pull/63), [`t3/review-github-actions-minutes`](https://github.com/densestjarl/mumble/tree/t3/review-github-actions-minutes), head `2f68ceb686e649dfdede1df4118f70f77288279f`.

The PR opened October 26 (UTC), last received commits October 26, and accumulated four implementation/documentation commits plus three review follow-ups around installer coverage, label-event cancellation and CodeQL artifacts. Its latest hosted attempt failed on a cancelled macOS ARM64 job with zero steps, after earlier successful attempts; the last review approved the fixes. This is a bounded, still-useful goal with no replacement PR. Older stability loops already shipped through squash-merged #40, and pop-out chat already has replacement #70.

**Salvage on current master.** The seven-file change is understandable and has a substantial offline test suite. All seven original commits were cherry-picked separately onto `origin/master` (`bc1ef1ed40072d163a781284ea859996e775b104`); `git range-diff` reports every patch unchanged and retains Adam Tarantino's recorded authorship. No old branch, PR or protected checkout was edited.

**Kept:** conservative installer selection, label isolation, all package/publication gates, both CodeQL comparison identities, original tests and seven-/one-day artifact retention. **Dropped:** no intended behavior; the old base and stale validation assumptions are replaced by a fresh build and test record. Runner migration, installer redesign, extra platform matrices and unrelated cleanup are outside this takeover.

## Verification

- **Build passed:** fresh worktree and recursive submodules; full non-unity Linux Debug client/server/plugins/tests build, 940 Ninja steps, GCC 13.3 and Qt 6.4.2. Assertions and project warnings-as-errors remain enabled. Twelve compiler warnings come from unchanged plugin sources. Screen sharing and chat WebM retained their defaults. [Configuration](configure.log.gz), [build log](build.log.gz), [manifest and results](validation.json).
- **CI behavior passed:** all 109 original selector/result-gate tests, eight publication tests, actionlint 1.7.12 and whitespace checks. Actionlint's optional shellcheck/pyflakes integrations were unavailable and disabled. [Focused logs](focused-checks.log.gz).
- **Live API selection passed:** source-only PR #69 skips packaging; the same payload with a simulated `ci:installers` event selects every installer; CMake-change PR #67 selects every installer. These checks only read GitHub data and did not modify labels. Offline invariants confirm the complete main platform matrix, all package/QR steps, existing publication steps/gates and both historical CodeQL jobs are preserved, with exactly one analysis and one compatibility upload. [Results](validation.json).
- **Full CTest completed:** 42 passed, 2 failed, 1 previously disabled suite, 45 registered, 247.50 seconds. File-transfer E2E passed in 246.97 seconds; real MainWindow, release/update, SQLite database and protocol suites passed. [Full log](ctest.log.gz), [summary](validation.json).
- **Real GUI smoke passed:** the built client and local server ran under Xvfb for 30.57 connected seconds. The client displayed both users, joined Interop, exchanged text in both directions, moved to Root and back, and received a final message. Both processes stayed alive throughout measurement; observer errors were zero. Client SIGTERM afterward was deliberate cleanup. [Connected](connected.png), [final message](soak-complete.png), [runtime results](validation.json).

### Baseline failures and limits

`TestFileTransferEngine::selectedPinSurvivesCacheRemoval` aborts on an empty-QList assertion. `TestChatWebM` fails `audioPlaybackAdvancesClockAndResumes`, `loopsAfterEnd`, `resumeAtClipEndRestartsImmediately` and `pauseResumeKeepsFrameSchedule`. These are the same signatures recorded in the [November 5 baseline controls](https://github.com/densestjarl/mumble/blob/takeover/pop-out-chat/docs/dev/takeover/2026-11-05-pop-out-chat/README.md). The complete `src`, `cmake`, `3rdparty` trees and root CMake file are byte-identical to current master; their Git object IDs are in [the manifest](validation.json). No separate master rebuild was performed in this takeover. Assertions and tests were not weakened, removed or disabled; OverlayTest retains its existing disabled state.

The CI feature is exercised by selector/gate tests and live API inputs; the GUI run is a general integration smoke check. It does not demonstrate physical audio/video quality. SQLite tests were enabled; online and MySQL/PostgreSQL service tests were omitted. Windows/macOS builds, actual package installation and new hosted CodeQL ingestion remain for CI. The old head's hosted Security checks passed; its final preview attempt [failed after macOS ARM64 was cancelled with zero steps](https://github.com/densestjarl/mumble/actions/runs/40349808984). A fresh PR must obtain its own hosted results.

### Reproduce

From a clean checkout with the documented dependencies:

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=cmake/mumble_dependency_provider.cmake \
  -DBoost_NO_BOOST_CMAKE=ON -DCMAKE_UNITY_BUILD=OFF \
  -Dclient=ON -Dserver=ON -Dtests=ON -Dwarnings-as-errors=ON \
  -Ddatabase-sqlite-tests=ON -Ddatabase-mysql-tests=OFF \
  -Ddatabase-postgresql-tests=OFF -Donline-tests=OFF \
  -Doverlay-xcompile=OFF -Dlto=OFF
cmake --build build -j 3
CTEST_PARALLEL_LEVEL=2 QT_QPA_PLATFORM=offscreen .github/workflows/test.sh build Debug
node --test scripts/tests/test_preview_installers.cjs
python3 -m unittest discover -s scripts/tests -p test_publish_preview.py
# Run with actionlint 1.7.12:
actionlint -shellcheck='' -pyflakes='' -oneline
# Use a new output directory. Requires pyatspi, Pillow, dbus and protobuf.
dbus-run-session -- xvfb-run -a -s '-screen 0 1280x800x24 -nolisten tcp' \
  /usr/bin/python3 scripts/compatibility/run_interop.py \
  --client "$PWD/build/mumble" --server "$PWD/build/mumble-server" \
  --output /tmp/mumble-ci-takeover-smoke-new --seconds 30
```

## Other stale work and delivery

[One recommendation for each of the 46 other stale branches](branch-recommendations.md). Preserve the selected old branch until the takeover merges, then delete it. No original branch or PR was deleted, merged, closed or rewritten. The protected `/home/agent/mumble` checkout remained on `codex/pop-out-chat` at `e4d1c0bac5b694854b0aa38381bb6b259b066c12`; sibling experiments were untouched. Only the fresh takeover worktree was used and it will be removed after publishing. The seven original patches and authorship are preserved in [the range comparison](range-diff.txt).
