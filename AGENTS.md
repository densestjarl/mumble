# Working in this repository

This fork welcomes AI-assisted and autonomous contributions. Agents are expected
to implement, verify, and finish the work requested by the user. Contributions
are evaluated on correctness, maintainability, and evidence of working behavior.

## Autonomy

- Use judgment for routine implementation choices and continue through fixes,
  validation, and delivery within the user's authorized scope.
- Commit, push branches, and create or update pull requests when those actions
  are part of the requested work. Authorized scheduled tasks may do the same.
  Respect any task-specific limits on publishing, merging, or deployment.
- Ask for clarification when essential information is missing or an action
  exceeds the authorization given. Do not require comprehension quizzes,
  repeated approvals, or a separate planning issue for routine work.
- AI-specific labels, commit trailers, and PR or issue disclaimers are optional.
  Describe the change and its validation clearly.

## Working tree and upstream sync

- Check the current branch, worktrees, and local changes before editing. Preserve
  unrelated changes, including dirty submodules; use an isolated worktree when
  the current checkout belongs to other work.
- `origin` is `gavmac11/mumble`; `upstream` is `mumble-voip/mumble`. Add the
  upstream remote if it is missing and fetch both before preparing a sync.
- Bring upstream `master` into a sync branch based on `origin/master` using a
  merge. Check for an existing sync PR before creating another. Preserve the
  fork's features and review fixes; avoid rebasing or force-pushing shared work.
- This file defines the fork's contributor policy. Preserve that policy during
  upstream merges, while incorporating useful technical guidance from upstream.

## Implementation

- Follow [CODING_GUIDELINES.md](CODING_GUIDELINES.md) and
  [COMMIT_GUIDELINES.md](COMMIT_GUIDELINES.md). Keep each commit focused on one
  logical change, with subjects such as `FIX(client): Handle disconnect cleanup`.
- Keep changes scoped to the task. Reuse existing code where appropriate and
  explain non-obvious constraints in comments. Update affected documentation.
- Before changing server concurrency or connection lifecycle code, read
  [MurmurLocking.md](docs/dev/MurmurLocking.md). UDP processing runs on the server's
  voice thread; most server control logic runs on the main thread. Check lock
  ownership and synchronous signal delivery to avoid races and deadlocks.
- Preserve protocol compatibility, authentication checks, and persisted settings
  unless the task calls for a deliberate, documented change.
- Keep side effects outside `assert()`, and use `[[maybe_unused]]` for values
  that are only needed by assertions in Debug builds.
- Format changed C/C++ code with the repository's `.clang-format`. Limit
  formatting to affected code and avoid unrelated changes under `3rdparty/`.

## Build and verification

Platform dependencies and instructions are in
[docs/dev/build-instructions](docs/dev/build-instructions/README.md), with options
in [cmake_options.md](docs/dev/build-instructions/cmake_options.md). Use CMake
3.28 or newer. Initialize required submodules in a clean checkout with
`git submodule update --init --recursive`; preserve existing local submodule work.

A Linux Debug configuration for client/server work, once dependencies are installed:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=cmake/mumble_dependency_provider.cmake \
  -DBoost_NO_BOOST_CMAKE=ON -DCMAKE_UNITY_BUILD=OFF \
  -Dclient=ON -Dserver=ON -Dtests=ON -Dwarnings-as-errors=ON \
  -Ddatabase-sqlite-tests=ON -Ddatabase-mysql-tests=OFF \
  -Ddatabase-postgresql-tests=OFF -Doverlay-xcompile=OFF -Dlto=OFF
cmake --build build -j 4
QT_QPA_PLATFORM=offscreen .github/workflows/test.sh build Debug
```

- Reuse a compatible build directory when available. Use a non-unity build to
  catch missing includes hidden by combined translation units.
- Run checks appropriate to the change. Use focused tests while iterating and
  the full suite for broad runtime changes or upstream merges. Documentation-only
  edits need content, link, and whitespace checks, not a full application build.
- Add regression coverage for substantive behavior changes. Diagnose failures;
  report baseline failures and environment limitations separately from regressions.
  Keep meaningful assertions enabled and report checks that were not run.
- MySQL/PostgreSQL tests require their services; online tests require network
  access. CI platform builds remain necessary for Windows and macOS coverage.

## Source map

- `src/mumble/`: client; `src/murmur/`: server.
- `src/`: shared networking, protocol, crypto, and database code.
- `src/tests/`: QtTest suites; `.github/workflows/`: CI commands and platform matrix.
- `plugins/`: positional audio; `overlay*/`: overlay implementations.
- `3rdparty/`: external dependencies, mostly submodules.
- `docs/dev/`: architecture, protocol, locking, and build documentation.
