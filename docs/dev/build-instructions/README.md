# Build Mumble

This documentation is version-specific. The current ``master`` branch contains the unstable code for a future release of Mumble.
If you want to build an already released stable version of Mumble, use the respective branch.


## Prerequisites

A fundamental dependency for building Mumble is [git](https://git-scm.com/), so make sure you have it installed. If you are new to git, make sure to
checkout [this guide](https://guides.github.com/introduction/git-handbook/) on git's basics in order to be able to follow the given instructions.

Furthermore Mumble requires a **Cpp20**-conform compiler.

You'll also need [`python`](https://www.python.org/downloads/). For detailed installation help, see [their Beginner's Guide](https://wiki.python.org/moin/BeginnersGuide(2f)Download.html).

Finally, you will need [`CMake`](https://cmake.org/) v3.28 or later.

## Checkout the code

The first step in building Mumble is to clone this repository and all the submodules via `git clone --depth 1 --recursive https://github.com/mumble-voip/mumble.git`.

_Tip:_ You can also [build a specific version or commit](faq.md#build-a-specific-version-or-commit) of Mumble.


## The Mumble Dependency Provider

Mumble provides its own CMake [dependency provider](https://cmake.org/cmake/help/latest/guide/using-dependencies/index.html#dependency-providers) that
can be used in case automatically downloading and building certain dependencies (instead of requiring them to be installed on your system) is
desirable. For more information, please read the [associated documentation](dependency_provider.md).


## Using the BUILD_NUMBER variable

We are currently using a [versioning scheme](https://www.mumble.info/blog/new-versioning-scheme/) that monotonically increases
the "build" or "patch" component of the version string. This build number is specified via cmake as a variable e.g. ``-DBUILD_NUMBER=123``.
Since we are exposing the complete version string in the Mumble UI as well as over the network protocol, it would be nice if the ``BUILD_NUMBER``
variable is set correctly when building the binaries. This is especially relevant for **distribution package maintainers**.
If not specified, the version string will end in ``.0``. This is also fine and not break anything, but it will be harder for
us to determine the version of Mumble in bug reports and usage statistics.

There exists a mapping between commit hash and ``BUILD_NUMBER``. If a ``BUILD_NUMBER`` exists for a given commit, it can be
retrieved from our website by requesting ``https://mumble.info/get-build-number?commit=<commit_hash>&version=<mumble_version>``
where ``<mumble_version>`` refers to the major and minor version of Mumble you are building e.g. ``1.6``. If there is no ``BUILD_NUMBER``
for a given commit, feel free to fallback to ``0``. There is also a [Python script](/scripts/mumble-build-number.py) which can be used
to perform this HTTP request automatically.


## CI build and test settings

The CI workflow disables link-time optimization (LTO) for Windows and macOS
pull-request verification builds. GCC verification retains LTO: GCC 13 otherwise
emits a `-Wstringop-overflow` diagnostic for `AudioOutputCache::loadFrom` in Release
unity builds, which fails the warnings-as-errors check. Builds on `master`, manual
CI builds, and preview installers also retain LTO. CodeQL builds disable LTO
because their executables are only used for analysis.
Windows/macOS server LTO-specific checks therefore run on `master` and manual CI
builds rather than before merging a pull request.

CI and preview installers run two CTest suites at a time. To use the same test
scheduling locally, run:

```sh
CTEST_PARALLEL_LEVEL=2 .github/workflows/test.sh build Debug
```

The suites that use the full client library share the `TestGuiRunner`
executable, avoiding repeated client LTO links. CTest still starts a separate
process for every suite, with the same names, timeouts and fixtures. Build that
executable with `cmake --build build --target TestGuiRunner`; run an
individual suite with `ctest --test-dir build -R '^TestAudioWizard$'` or
`build/tests/TestGuiRunner TestAudioWizard -platform offscreen`.
Use `build/tests/TestGuiRunner --list` to list the compiled suites.

The end-to-end file-transfer and server-database suites have explicit CTest costs
so fresh CI runners start the longest work first when running tests in parallel.

`TestDatabase` and `TestServerDatabase` share a CTest resource lock because they
reset the same MySQL/PostgreSQL database. Keep this lock on any additional test
that uses that database. Other tests use separate fixtures or temporary storage.

Compiler caches restore from the pull request's base branch. Windows/macOS PR
verification uses different compiler flags from LTO-enabled `master` builds, so
the first push cannot reuse those compiled objects. Fork PRs remain restore-only.
Only same-repository Windows CI jobs save PR-specific snapshots for later pushes;
other PR jobs restore caches without saving, reducing cache storage pressure.
Windows PR caches are limited to 1 GB, while non-PR Windows CI caches allow 2 GB.
The cleanup workflow deletes merge-ref caches when a same-repository PR closes.

Cache-saving jobs use timestamped keys and can save after failed or cancelled
runs, allowing later builds to reuse completed compilations. Dependency archive
caches remain keyed by their dependency versions.
The macOS/Linux preview compiler keys use a new version prefix so older fixed-key
snapshots cannot mask refreshed caches. Those old keys remain restore fallbacks
for the first build after migration.

## Specific Instructions

In order to actually build Mumble, you can follow one of the following instruction sets:
- [Build on Windows](build_windows.md)
- [Build on Linux](build_linux.md)
- [Build on macOS](build_macos.md)
- [Static build](build_static.md)


Furthermore you might find these helpful as well:
- [FAQ](faq.md)
- [Determine build number](find_build_number.md)
- [Common build errors](common_build_errors.md)
- [List of available cmake options](cmake_options.md)
- [IDE integration](ide_integration.md)
- [Create an installer](build_installer.md)
- [Setup Visual Studio](setup_visual_studio.md)
