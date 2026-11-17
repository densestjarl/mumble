# Compile standalone file-transfer support once

Five standalone suites used to compile overlapping copies of the same twelve
backend sources. A shared static test-support library builds those sources and
Qt meta-objects once. Each suite retains its own executable, QtTest entry point,
CTest registration, timeout, fixtures and test source. The production client
continues to build its own backend with its existing feature settings.

Same GCC 13.3, Qt 6.4.2, Linux Debug non-unity configuration, four jobs, no
compiler cache; cached dependency source downloads reused from the documented
`~/.cache/mumble-sync-build/2026-10-21/_deps` cache. The five measured targets are
TestPQCrypto, TestFTIdentity, TestFileTransferSession, TestFileTransferEngine,
and TestFileTransferE2E. A target clean also removes their shared/server
prerequisites; it is not a clean build of the entire application.

| Measurement | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| Clean five-target build, median of two | 40.37 s | 28.41 s | 29.6% |
| Clean range | 40.34–40.41 s | 28.22–28.61 s | |
| Backend C++ compilations in that clean build | 49 | 12 | 75.5% |
| All C++ compilations in that clean build | 100 | 64 | 36.0% |
| Five-target rebuild after touching Merkle.cpp, median of three | 3.302 s | 2.000 s | 39.4% |
| Source-edit range | 3.269–3.460 s | 1.994–2.029 s | |
| Recompiled Merkle.cpp translation units | 5 | 1 | 80.0% |

[Samples](file-transfer-build-samples.json),
[environment](file-transfer-build-environment.json),
[baseline clean logs](file-transfer-baseline-clean-logs.json.gz),
[candidate clean logs](file-transfer-after-clean-logs.json.gz),
[baseline source-edit logs](file-transfer-baseline-incremental-logs.json.gz),
[candidate source-edit logs](file-transfer-after-incremental-logs.json.gz).
The extra Qt meta-object compilation for the new library is included in the
all-C++ count and timings. Windows/macOS and unity/LTO timings are not inferred
from these measurements. Existing open PR #67 handles a different issue in the
client UI build graph; its changes were not applied.

## Reproduce

Use the Debug configuration in AGENTS.md with four jobs and a fully built Ninja
directory, then run the new harness on baseline and candidate, serially on the
same idle machine:

```sh
python3 scripts/performance/measure_file_transfer_build.py \
  --build build --source src/mumble/PQFileTransfer/crypto/Merkle.cpp \
  --output /tmp/ft-incremental --repeat 3 --jobs 4
python3 scripts/performance/measure_file_transfer_build.py \
  --build build --clean --output /tmp/ft-clean --repeat 2 --jobs 4
```

Copy the harness to baseline `bc1ef1ed40072d163a781284ea859996e775b104` before
running it there, and use separate output directories. The source-edit mode
preserves file contents and restores its timestamp; clean mode uses Ninja's
five-target dependency closure. Reconfiguration/warmup builds are excluded.
The baseline target graph and source inputs were unchanged by the independent
video reassembly experiment: none of these five targets links the GUI client
or its receiver regression fixture.

## Verification

Production C++ and test source files are unchanged. All assertions,
cryptographic work, relay/network paths, transfer sizes and test process
isolation remain. The known baseline failures are TestChatWebM's four headless
playback/clock cases and TestFileTransferEngine::selectedPinSurvivesCacheRemoval's
empty QList assertion. The independent receiver fixture was present in the
baseline full CTest run (43 passes, two baseline failures, one pre-disabled
OverlayTest, 242.61 s); this build-only PR retains the original 45-suite
registration. SQLite tests enabled; online/MySQL/PostgreSQL omitted.

The full clean client/server/plugins/tests build passed in **288.73 s** with
warnings-as-errors and assertions enabled. The initial full clean baseline was
364.03 s. These single observations include warm-cache and scheduling effects;
the repeated scoped measurements above support the speedup claim. Both full
builds emitted the same 12 existing conversion warnings in unchanged positional
audio plugins; the new library introduced none.

Full candidate CTest: **42 passed, the same two baseline failures, one
pre-disabled OverlayTest**, 248.63 s. The E2E transfer suite passed in 240.65 s.
The remaining engine cases also passed separately on the candidate (61
invocations). No failing tests or meaningful assertions were disabled. This is
not a CTest speedup. The eight-source and four-source unity batches were also
syntax-checked with the actual new target's compiler flags and warnings as
errors, matching CI's default unity batch size; platform CI execution is still
required.

[Validation summary](file-transfer-validation.json),
[full clean build](test-support-full-build.log.gz),
[baseline CTest](file-transfer-baseline-ctest.log.gz),
[candidate CTest](test-support-ctest.log.gz),
[remaining engine cases](test-support-engine-remaining.log.gz),
[unity batch check](test-support-unity.log.gz).
