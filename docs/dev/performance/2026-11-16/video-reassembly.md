# Video fragment reassembly

Ordered fragments previously rescanned every stored fragment after each arrival.
A count of uniquely stored fragments replaces that quadratic completeness check.
Duplicate handling, the mutex, metadata checks, frame/window limits, assembly
copies, decoding and wire format are unchanged.

Same Linux non-unity Debug configuration, GCC 13.3, Qt 6.4.2, four build jobs,
no compiler cache; dependency source downloads reused from the documented
`~/.cache/mumble-sync-build/2026-10-21/_deps` cache. See [environment](environment.json).
Each fragment carries 1,024 bytes. Reassembly uses an unsupported codec to
exclude decoding, but includes the real receiver's allocations, fragment copies,
assembly and cleanup. A separate regression decodes a real H.264 IDR delivered
in reverse order with duplicates, and checks its dimensions and red pixels.

Three alternating baseline/candidate pairs, five QtTest median runs per sample,
32 frames per iteration batch:

| Reassembly per frame | Baseline median (range) | Candidate median (range) | Reduction |
| --- | ---: | ---: | ---: |
| 1,024 ordered fragments / 1 MiB | 4.313 ms (4.313–4.344) | 0.312 ms (0.312–0.312) | 92.8% |
| 4,096 ordered fragments / 4 MiB | 65.688 ms (65.594–65.781) | 1.250 ms (1.250–1.656) | 98.1% |
| 4,096 reversed fragments, control | 1.313 ms (1.281–1.719) | 1.281 ms (1.250–1.594) | No claim |

The [control summary](reassembly-controls.json) and
[raw QtTest XML](reassembly-control-samples.json.gz) retain all samples.
These are reassembly-only gains for large frames, not whole-client speedups.
Smaller frames and reversed delivery already avoid most of the old scan cost.
The new count fits the same bounded reassembly lifecycle, including duplicate,
invalid-metadata, oversized-payload and sender-reset paths covered by the tests.

## Reproduce

Build with the Debug configuration in AGENTS.md. Use `c683198ebb12760c894df8650ff0be5763c380df`
(the test-only first commit) as baseline, and the PR head as candidate. In each
revision, run the same command on an idle machine:

```sh
QT_QPA_PLATFORM=offscreen MUMBLE_BENCHMARK_REASSEMBLY=1 \
  build/tests/TestGuiRunner TestScreenShareReceiver \
  reassemblyBenchmark:128-0 reassemblyBenchmark:1024-0 \
  reassemblyBenchmark:4096-0 reassemblyBenchmark:4096-1 \
  -iterations 32 -median 5 -o result.xml,xml -o result.txt,txt
```

Repeat in alternating order three times. The suffix is fragment count and
arrival order: 0 = ordered, 1 = reversed, 2 = a fixed shuffled order. Running
`reassemblyBenchmark` alone measures all twelve combinations with automatic
iteration calibration. Those exploratory results are excluded from the table;
fixed batches keep allocation/iteration history comparable in the controls.
Normal CTest runs skip the timing cases unless the environment variable is set.

## Streaming context and verification

A real disposable loopback server plus two Qt pipeline clients under Xvfb
captured, encoded, relayed and decoded all 165 frames per run (15 warmup,
150 measured), for three serial runs per revision. These clients use the
production screen-capture/packetizer/receiver paths and TLS tunnel; they omit the
full application UI, preview widgets, webcam hardware, audio and UDP pacing.
CPU percentages use one logical CPU as 100%. RSS is process peak for clients
and sampled RSS for the server. Allocation requests are instrumented calling-thread requests,
not live heap or a complete count of copies. The reassembly path still copies
each payload into fragment storage and once into the assembled buffer: two
payload-sized copies per complete frame on both revisions.

[Baseline](streaming-baseline.json), [candidate](streaming-reassembly.json), and
[per-frame samples](streaming-samples.json.gz) record CPU, RSS, codec and frame
time, allocations, event-loop gaps, encoded bytes and tunnel bytes per frame.
Identical input RGB hashes were confirmed. Encoder/decoded hashes vary even
between unchanged runs; bit-exact codec output is not claimed. The ordinary
screen workload does not establish improvements in CPU/RSS, codec time,
allocation/copy traffic, event-loop stalls or network volume. None is claimed.
Two accidentally overlapping exploratory runs were discarded before collecting
the serial baseline; they are excluded from every published summary.

The optional pipeline harness is already part of open PR #66 and is not
reintroduced here. Use its `scripts/performance` files at commit
`5e3e766202ca5b083e00da083db79a10b38f8102`, following its README. The small
[measurement patch](streaming-measurement.patch) accumulates allocation counts
across all receiver fragments instead of recording only the completing packet.
Apply it in the extracted harness directory with `patch -p1`. Add that directory
locally with `add_subdirectory("<harness>" "local-pipeline-benchmark")` in
`src/tests/CMakeLists.txt`; no packetizer optimization is applied. Remove the
local stanza before publishing. The same harness and patch were used on both
revisions. The fixture raises server video ceilings to avoid unpaced-IDR drops;
this is not a bandwidth-policy or production-pacer measurement.

Full client/server/plugins/tests build passed. Full CTest: 43 passed, the same
two baseline failures, one pre-disabled OverlayTest, on both revisions.
Baseline wall time 242.61 s; candidate 247.54 s. No CTest speedup is claimed.
TestChatWebM fails its four existing headless audio-clock/playback cases;
TestFileTransferEngine aborts in selectedPinSurvivesCacheRemoval on an empty
QList assertion. All other engine cases also passed separately on baseline
(61 invocations). Assertions and failing cases remain enabled. SQLite ran;
online/MySQL/PostgreSQL tests were not configured. Windows/macOS remain for CI.

[Baseline CTest](baseline-ctest.log.gz), [candidate CTest](reassembly-ctest.log.gz),
[initial build](reassembly-build.log.gz), [final published-tree build](reassembly-final-build.log.gz), [remaining baseline engine cases](baseline-engine-remaining.log.gz).
The initial clean full build took 364.03 s; the 134.65 s candidate rebuild is
validation only, not a comparable clean-build measurement. Existing open PRs
#66 (packetization), #67 (unrelated UI rebuilds), and #63 (CI duplication) were
left untouched.
