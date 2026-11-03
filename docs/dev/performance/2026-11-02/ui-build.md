Client UI headers now use Qt's `qt6_wrap_ui` with individual outputs, rather than the client-wide AUTOUIC stamp. Forms are collected after all platform/feature-specific `target_sources` calls. The same 39 forms and the same Qt uic executable are used; all generated header contents are byte-identical.

On CMake 3.28.3/Ninja, an unrelated C++ edit made the old graph's UI-header phony outputs dirty and recompiled 102 C++ translation units. The new graph recompiles only the two copies of the edited packetizer source. No production C++ behavior, form contents, layout-state hashes, or test assertions changed.

This is an incremental build measurement after touching `src/mumble/VideoFramePacketizer.cpp`, with contents and original timestamp preserved. Both revisions use the same non-unity Debug client/server/plugins/tests configuration, two jobs, cached dependency source downloads, and no compiler cache. Two baseline runs and three candidate runs:

| Measurement | Before (range) | After (range) |
| --- | ---: | ---: |
| Full incremental build wall time (s) | 208.84 (208.71–208.98) | 9.97 (9.63–10.08) |
| Build CPU time (s) | 409.79 (409.36–410.22) | 14.60 (14.48–14.63) |
| C++ compilations | 102 | 2 |
| Actual UI generations | 0 | 0 |

The wall-time reduction is **95.2%** (208.84 to 9.97 seconds); CPU time falls 96.4%. The run ranges do not overlap. This removes unnecessary consumer recompilation, not UI generation work. No clean-build or CTest speedup is claimed.

See the [reproduction script](../../../../scripts/performance/run_build_benchmark.py), [instructions](../../../../scripts/performance/BUILD.md), [baseline](ui-before.json), [candidate](ui-after.json), [raw build logs](ui-build-samples.json.gz), and [environment](ui-environment.json).

Validation removed the stale AUTOUIC headers before the full build so they could not hide missing dependencies. The full client/server/plugins/tests build passed (287.65 seconds, an incremental reconfiguration build, not a clean-build comparison). [Baseline](ui-header-baseline.json) and [candidate](ui-header-after.json) SHA-256 manifests match for all 39 generated headers. Touching `FileTransferConfig.ui` generated exactly its own header and rebuilt two C++ files; all 39 output hashes still matched ([regression result](ui-edit-regression.json), [log](ui-edit-regression.log.gz)). The AudioWizard, VideoViewUi, and FileTransferUi focused suites passed after regeneration.

[Full CTest](ui-ctest.log.gz): 42 passed, the same two baseline failures, one pre-existing disabled suite; 279.68 seconds versus 277.16 seconds in the [baseline](ui-baseline-ctest.log.gz). The failures are TestChatWebM's audio-clock/timing cases and TestFileTransferEngine::selectedPinSurvivesCacheRemoval's QList assertion. Online and MySQL/PostgreSQL tests were not configured; SQLite tests ran. Windows/macOS and other Qt/CMake versions remain for CI coverage. The first clean baseline build took 572.40 seconds; no comparable candidate clean-build measurement was made.
