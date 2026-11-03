# Reproducible Linux video measurements

Build the regular client, server and tests first. Then enable this opt-in
Linux/glibc harness in the same CMake directory:

```sh
cmake -S . -B build -Dvideo-pipeline-benchmark=ON
cmake --build build --target BenchmarkVideoPipeline video_allocations -j 2
xvfb-run -a -s '-screen 0 1920x1080x24' \
  python3 scripts/performance/run_video_pipeline.py \
  --client build/tests/BenchmarkVideoPipeline \
  --allocator build/src/tests/video-pipeline-benchmark/libvideo_allocations.so \
  --server build/mumble-server --output /tmp/video-baseline --repeat 5
```

Use the identical command and build configuration on the candidate revision,
changing only the output directory. Each sample creates a temporary loopback
server database and runs two Qt pipeline clients under Xvfb: the sender calls
the production `ScreenCapture::captureFrame()` and `packetizeFrame()`, and the
receiver passes the server's relayed video into `ScreenShareReceiver`. A fixed
1920x1080 desktop scene with a moving rectangle makes input reproducible. Every
sample requires all 165 frames to arrive and decode, discards 15 warmup frames,
and measures 150 frames at 15 fps. The clients time out on missing frames.

These are small pipeline clients, not the complete Mumble application. They
exercise screen capture, H.264 encoding, fragmentation, TLS tunneling, server
relay, reassembly and decoding. They do not exercise webcam devices, UDP pacing,
audio, preview widgets, or the complete application UI. The fixture raises the
server's video ceilings to 20 Mbit/s to avoid dropping the initial unpaced IDR;
it measures relay/codec work, not the production pacer or bandwidth policy.
Do not extrapolate the
CPU figures to those workloads.

`summary.json` reports the median and range across runs. Raw per-frame JSON
remains in each sample directory. Measurements include:

- Sender frame time: screen grab, image conversions and H.264 submission up to
  `frameEncoded`, excluding packetization and hashing. Receiver frame time:
  accumulated reassembly/decode/RGBA conversion time up to `frameDecoded`,
  excluding pixel hashing. The interposer also records isolated codec wall time
  inside FFmpeg's `avcodec_send_*` and `avcodec_receive_*` calls. This includes
  any codec worker waits, and excludes the image conversion and screen grab.
- Sender/receiver CPU from `getrusage()` after warmup, and process peak RSS.
  Server CPU and RSS come from `/proc`; its interval includes warmup and client
  startup. CPU percentage uses one logical CPU as 100%.
- Allocation calls and requested bytes on the capture thread up to
  `frameEncoded`, including aligned FFmpeg allocations. The glibc interposer
  counts allocation requests, not live heap size or codec worker allocations.
  Requests of at least 1 MiB are also counted separately. Together with the
  source formats, these identify full-frame allocations/copies without sampling
  allocator internals. Reallocations count the requested size, not net growth.
  Packetization has a separate allocation scope and timer, excluding the
  caller's socket writes and returned packet destruction.
- Main event-loop callback gaps from a precise 5 ms timer. These include fixture
  drawing and validation work; compare like-for-like rather than treating them
  as complete application UI latency.
- Encoded payload bytes and tunnel bytes (protobuf/type byte plus six-byte TCP
  framing). TLS/TCP/IP overhead is excluded. Output SHA-256 hashes cover every
  measured encoded frame and the first/last measured decoded frames. Hashing
  every 8 MiB decoded frame would dominate receiver CPU, so pixel validation is
  sampled. A separate RGB hash of the actual X11 scene's first/last measured
  frames verifies identical input. Bitstream/pixel hashes are diagnostic and can vary even between unchanged
  baseline runs. Check repeatability before claiming bit-exact output.

Output directories contain a disposable server's private certificate/database;
publish only the measurement JSON and sanitized logs. Keep artifacts outside the
worktree if it will be removed.
