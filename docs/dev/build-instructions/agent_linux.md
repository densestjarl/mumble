# Linux agent workflow

Use a separate worktree when another checkout may contain unfinished work.
Run these commands from a repository checkout; replace the branch and path with
unused names. Do not reset another checkout or its submodules.

```sh
git fetch origin master
git worktree add -b my-change /tmp/mumble-my-change origin/master
cd /tmp/mumble-my-change
git submodule update --init --recursive
```

## Dependencies and build

Follow the distribution package list in [Building on Linux](build_linux.md)
first. Use CMake 3.28+, a C++20 compiler, Ninja, pkg-config and Python 3. The
fork's default video/WebM features additionally require these Ubuntu packages:

```sh
sudo apt install ninja-build ccache \
  libavcodec-dev libavutil-dev libswscale-dev libavdevice-dev \
  libavformat-dev libswresample-dev libpipewire-0.3-dev
# Optional Qt WebM audio integration (otherwise uses Mumble's audio output):
sudo apt install qt6-multimedia-dev
# Only needed for the headless smoke test below:
sudo apt install xvfb xauth ffmpeg openssl
```

The FFmpeg encoder must include `libx264` for screen sharing. Check with
`ffmpeg -hide_banner -encoders`; installing the `ffmpeg` executable alone does
not supply development headers. A successful compile does not prove that an
encoder, audio device or camera is usable at runtime.

```sh
scripts/dev/build-linux.sh "$PWD/build-agent"
CTEST_PARALLEL_LEVEL=2 .github/workflows/test.sh build-agent Debug
```

The helper refuses dirty or locally advanced submodules, initializes missing
submodules, configures a non-unity Debug build of both
client and server with assertions, warnings as errors and tests, then builds
with four jobs. Override `BUILD_JOBS` for memory limits. Screen sharing, WebM
chat and file sharing retain their defaults. SQLite tests run without external
services; MySQL/PostgreSQL and online tests are disabled. Their backends can
still be compiled, so the platform dependency list remains relevant.

The dependency provider downloads pinned compatible dependencies if system
packages do not provide the expected targets/versions (notably SOCI). First
configure needs network access. A bare `cmake -B build` is not equivalent to
this recipe. Extra CMake options can follow the build directory argument.

## Incremental work and compiler cache

Keep a build directory for the same worktree and toolchain. For later edits:

```sh
cmake --build build-agent --target mumble --parallel 4
cmake --build build-agent --target TestGuiRunner --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build-agent --output-on-failure \
  --no-tests=error -R '^(TestVideo.*|TestChatWebM)$'
```

`TestGuiRunner` links the client library once and contains many suites; building
an individual suite's object target does not relink the runner. Use `ctest -N`
to list registered tests and `build-agent/tests/TestGuiRunner --list` to list
its suites. The workflow test wrapper sets `QT_QPA_PLATFORM=offscreen`, reports
failures and writes JUnit output under `build-agent/test-results/`.

When ccache is installed, the build helper uses
`${XDG_CACHE_HOME:-$HOME/.cache}/mumble/ccache`, or your `CCACHE_DIR` override.
Choose a cache size appropriate to free disk space, for example
`CCACHE_DIR="$HOME/.cache/mumble/ccache" ccache --max-size 3G`.
Debug paths map the source root to `.` so compilations can be reused between
clean worktrees. Run a debugger from the source root, or configure its source
mapping. When invoking `cmake --build` directly, export the same settings:

```sh
export CCACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/mumble/ccache"
export CCACHE_BASEDIR="$PWD" CCACHE_NOHASHDIR=true
ccache --show-stats
```

Compiler flags, generated files and dependency paths can still cause misses;
measure hits instead of assuming every object will be reused. Do not share one
CMake build directory across different source worktrees: it stores absolute
source paths. Compiler cache storage can be shared.

## Real client/server smoke test

```sh
python3 scripts/dev/headless-smoke.py build-agent /tmp/mumble-smoke-new
```

The output directory must not exist. The script creates a private Xvfb display,
binds a server to loopback on a dynamically chosen port, launches the real Qt
client and requires an authenticated connection in the server log. It saves
`connected.png`, `client.log` and `server.log`, and terminates both processes
on success or failure. Startup waits are bounded. A port collision fails with
logs; rerun with a new output directory. It uses `QT_QPA_PLATFORM=xcb`, since
the offscreen plugin used for unit tests does not exercise X11 capture.

Settings, databases, temporary files, XDG directories and a test certificate are isolated
under the output directory. Only that certificate is trusted in the temporary
client database, so no certificate-dialog clicks or changes to normal client
settings are required. The generated identity is only for local testing;
delete the output directory after retaining the logs/screenshots you need.

This proves startup, TLS authentication and connected UI rendering. It does not
prove audible output, microphone input, webcam capture, screen/video delivery,
Wayland portal permissions, or native macOS/Windows integrations. A screenshot
is evidence to inspect, not an assertion about individual UI controls. Video
packetization, pacing, quality, UI and WebM suites cover additional behavior;
a two-client media exercise is still needed for end-to-end video delivery.

For that exercise, use two isolated client profiles with distinct client
databases on the same Xvfb display. Connect both to this fork's server, choose
Self → Share video → Screens in the sender, and View Screen Share from the
sender's user context menu in the recipient. Inspect changing decoded frames
in the recipient gallery. Official servers do not announce this fork's video
relay capability. Xvfb can exercise X11 screen capture and software encoding;
it cannot substitute for a webcam or Wayland desktop portal session.

After pushing committed changes, remove only worktrees you created. Initialized
submodules require `git worktree remove --force <path>`; check tracked and
submodule changes before using it. Preserve evidence and the shared compiler
cache outside the worktree.
