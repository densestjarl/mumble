Video packetization now constructs one protobuf message per encoded frame and assigns each fragment directly into its mutable payload string. Immutable metadata is set once; payload capacity is retained between fragments. This removes temporary strings from protobuf's bytes setter without changing wire bytes, frame lifetime, pacing, or protocol fields.

The table shows medians and run ranges from five independent loopback samples per revision. Each sample decodes all 165 frames, discards 15 warmup frames, and measures 150 frames from the same Xvfb scene. Both revisions have the same captured RGB hash. See the [harness and metric definitions](../../../../scripts/performance/README.md), [environment](environment.json), [baseline](packetizer-baseline.json), [candidate](packetizer-after.json), and [raw samples](packetizer-samples.json.gz).

| Measurement | Before (range) | After (range) |
| --- | ---: | ---: |
| Packetization time (microseconds/frame) | 36.09 (35.28–37.13) | 32.36 (31.55–32.51) |
| Packetization allocation calls/frame | 78.31 (78.23–78.44) | 22.35 (22.33–22.35) |
| Requested packetization bytes/frame | 52,327.59 (52,291.35–52,345.51) | 18,669.01 (18,666.20–18,690.77) |
| Capture + conversion + encode (ms/frame) | 13.98 (12.29–14.24) | 14.05 (12.28–14.06) |
| Codec encode calls (ms/frame) | 2.10 (2.07–2.13) | 2.09 (2.07–2.10) |
| Sender CPU (% of one logical CPU) | 37.79 (37.69–38.44) | 38.08 (37.33–38.15) |
| Sender peak RSS (MiB) | 180.54 (180.37–180.79) | 188.28 (181.09–188.43) |
| Sender event-loop gap, p99 (ms) | 20.92 (17.01–21.17) | 20.78 (16.58–21.00) |
| Reassembly + conversion + decode (ms/frame) | 2.62 (2.59–2.64) | 2.63 (2.61–2.65) |
| Codec decode calls (ms/frame) | 2.14 (2.12–2.17) | 2.16 (2.15–2.17) |
| Receiver CPU (% of one logical CPU) | 5.67 (5.64–5.76) | 5.74 (5.67–5.77) |
| Receiver peak RSS (MiB) | 97.24 (97.09–97.66) | 97.34 (97.18–97.58) |
| Encoded payload bytes/frame | 16,952.35 (16,940.33–16,957.66) | 16,939.62 (16,938.35–16,960.91) |
| Tunnel bytes/frame | 17,418.71 (17,406.49–17,424.51) | 17,406.11 (17,405.34–17,427.85) |
| Server CPU (%) | 0.63 (0.63–0.72) | 0.63 (0.63–0.63) |
| Server RSS (MiB) | 41.05 (40.84–41.17) | 41.21 (40.86–41.27) |

The demonstrated wins are 71.5% fewer packetization allocation calls, 64.3% fewer requested bytes, and 10.4% less packetization wall time. These are allocation requests, not live heap size. Whole-client CPU, RSS, codec time, event-loop gaps, and network traffic have no claimed improvement. Peak RSS moved between roughly 180 and 188 MiB even for unchanged controls; the [alternating controls](packetizer-controls.json) have overlapping RSS ranges and confirm the isolated packetization timing gain. Their baseline binary uses the same client library and compiler flags with only the packetizer object restored to origin/master.

The fixture uses two small Qt pipeline clients and a real local server under Xvfb, not the complete Mumble UI. It uses TLS tunneling and 20 Mbit/s fixture video ceilings to avoid dropping an unpaced startup IDR. It does not measure webcam hardware, UDP pacing, audio playback, preview rendering, Windows, or macOS. Encoded/decoded hashes varied between unchanged baseline runs; bit-exact codec output is not claimed. Regression tests instead compare every serialized fragment with a fresh protobuf message for tiny frames, boundary sizes, short tails, and large keyframes, with both keyframe flag values.

The first clean non-unity Debug client/server/plugins/tests build took 572.40 seconds using two jobs, with cached dependency source downloads reused. Benchmark setup caused additional cache invalidation, so subsequent rebuild times are validation timings and are not comparable clean-build speedups. The final full build passed (209.81 seconds). The focused packetizer suite passed. [Full CTest](packetizer-ctest.log.gz) took 276.70 seconds versus [277.16 seconds at baseline](baseline-ctest.log.gz): 42 passed, two failed, and one was already disabled in both. The unchanged failures are the four TestChatWebM audio-clock/timing cases and TestFileTransferEngine::selectedPinSurvivesCacheRemoval's QList assertion. No tests or assertions were disabled. Online and MySQL/PostgreSQL tests were not configured; SQLite tests ran. No CTest speedup is claimed.

A rejected screen-capture conversion experiment halved large allocation requests from 31,104,000 to 16,588,800 bytes/frame, but three alternating controls raised sender CPU from 37.64% to 45.34% and mean capture/encode time from 14.38 to 16.93 ms. That [patch](rejected-capture.patch) was reverted; [control results](rejected-capture.json) are retained. Fewer copies alone did not justify landing it.

Further profiling targets are moving screen encoding off the UI thread with bounded frame queues, fragment-completeness scans on large frames, hidden webcam preview conversion, and chat WebM playback after the headless audio-clock fixtures are made reliable. Build opportunities include explicit generated UI header outputs, preserving the unchanged SpeexDSP generated header timestamp, and avoiding duplicate file-transfer source compilations in standalone tests. These are opportunities, not measured gains in this patch.
