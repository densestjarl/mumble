# Upstream sync evidence — 2026-11-04

**COMPAT BREAK:** official stable v1.5.939 cannot open the fork's schema-11 database. This is an existing limitation inherited from official master, which reproduces the same failure. All four mixed client/server protocol and GUI sessions pass.

## Revisions and conflict resolutions

The user authorized `densestjarl/mumble` after verifying that the old `gavmac11/mumble` URL redirects there. Official `mumble-voip/mumble` was used only for reads. Initial fork ancestry: **173 ahead / 40 behind** upstream `08ac9b7ee87ad996eb7b4238bc57f160d0b01fce`. Fork master is `bc1ef1ed40072d163a781284ea859996e775b104`.

Existing sync PR #65 was reused. No competing active sync run existed, and its remote head was verified as `843c59fb81b9cc825acf6e282eb95a273cb019c8` before work and before publication. A fresh worktree under `~/.cache/mumble-sync-worktrees/2026-11-04` was created with `git worktree add`, then its submodules were initialized recursively. Existing worktrees and local changes were left intact.

Merge `d01f68d0c` incorporates current fork master, including the merged file-transfer test setup optimization. Merge `7aec046f5` incorporates latest upstream master. Both are ordinary two-parent merges; existing PR commits and review fixes remain ancestors. The PR's cumulative upstream range is `c7a8993ca..08ac9b7ee` (40 commits; [list](merge-range.txt)); this update adds `162f42805..08ac9b7ee` (22 commits; [list](new-merge-range.txt)). No upstream commits remain missing from the sync branch. The main fork branch was not pushed or merged.

Five conflicted files required resolution ([resolved diff](resolution-review.diff.gz)):

- `.github/workflows/build.sh`: combine the fork's test, packaging and LTO controls with upstream's macOS disk-image packaging.
- `.github/workflows/build.yml`: retain the fork's four Linux/Windows/Apple Silicon/Intel lanes and bounded Windows PR caches, add upstream's restored static Ubuntu 26.04 lane and `.dmg` artifact. Keep the fork's regression jobs and artifact behavior rather than replacing its matrix with all upstream variants.
- `.github/workflows/code-ql.yml`: retain fork CodeQL scans and historical comparison coverage despite upstream deleting its workflow. Security scanning is a deliberate fork workflow.
- `ipcutils/IPCUtils.cpp`: adopt upstream's portable per-user Linux temporary fallback and macOS temporary directory behavior, preserving fork socket-name helpers. The older Darwin-specific fallback is superseded by upstream's implementation. Update `TestIPCUtils` to cover set, unset and empty XDG variables, directory creation and consistent socket/overlay paths. macOS expects the temporary path even when XDG is set.
- `src/mumble/VersionCheck.cpp`: retain the fork's release/cache update checker. Upstream only changes a macOS macro inside its obsolete legacy updater; the corresponding code no longer exists in the fork.

Other upstream changes include protobuf config-mode preference with module fallback, macOS macro cleanup, local-network permission metadata and ad-hoc application signing. Fork video/screen sharing, chat media, file transfer, AI tooling and AI-friendly contributor policy are preserved. [Source/policy comparison](baseline-source-comparison.json). No protocol or database migration changes were introduced by the merge.

## Compatibility matrix

| Check | Result | Evidence |
|---|---|---|
| Protocol against official master | PASS | [descriptor and transport ID audit](protocol-master.json) |
| Protocol against stable v1.5.939 | PASS | [descriptor and transport ID audit](protocol-stable.json) |
| PROTOCOL.md / version negotiation / capability gating | PASS | [static review](protocol-review.md), [protocol diff](protocol.diff.gz) |
| Official master client → fork server | PASS, 125.30 s | [result](interop/master-client/result.json), [screenshot](interop/master-client/soak-complete.png) |
| Fork client → official master server | PASS, 125.41 s | [result](interop/master-server/result.json), [screenshot](interop/master-server/soak-complete.png) |
| Official v1.5.939 client → fork server | PASS, 125.44 s | [result](interop/stable-client/result.json), [screenshot](interop/stable-client/soak-complete.png) |
| Fork client → official v1.5.939 server | PASS, 125.29 s | [result](interop/stable-server/result.json), [screenshot](interop/stable-server/soak-complete.png) |
| Fork-to-fork identity and capability controls | PASS, 125.70 s | [result](interop/fork-positive/result.json), [screenshot](interop/fork-positive/soak-complete.png) |
| Official master DB/config → fork | PASS | [result](database/master-to-fork/result.json), [log](database/master-to-fork/server.log.gz) |
| Fork DB/config → official master | PASS | [result](database/fork-to-master/result.json), [log](database/fork-to-master/server.log.gz) |
| Official stable DB/config → fork | PASS | [result](database/stable-to-fork/result.json), [log](database/stable-to-fork/server.log.gz) |
| Fork DB/config → official stable | **FAIL: schema 11 → 9** | [result](database/fork-to-stable/result.json), [log](database/fork-to-stable/server.log.gz) |
| Official master DB → official stable (control) | **FAIL: schema 11 → 9** | [result](database/master-to-stable/result.json), [log](database/master-to-stable/server.log.gz) |

Each native GUI ran under Xvfb, showed GuiInterop and SuperUser, joined Interop, exchanged text in both directions, moved to Root and back, stayed connected for 125+ seconds, and received a final text message. Observer errors were empty and no measured process crashed or disconnected. All five final screenshots were inspected. Fork clients kept video/file controls disabled on official servers and showed no identity prompt. The fork-to-fork fixture enabled both controls and announced file/receipt capabilities without a passphrase prompt. Clients were muted/deafened; these are GUI/control/protocol tests, not physical media quality tests.

All client/server cleanup completed within the fixture timeout in this run. Client SIGTERM exits after the final screenshot are intentional, separate from measured liveness. Headless logs contain expected audio/PipeWire configuration and SocketRPC warnings; desktop D-Bus channel operations and text delivery passed.

## Build and tests

Full Linux Debug client/server/plugins/tests build passed with project warnings-as-errors, assertions enabled, unity/LTO off, screen sharing/file sharing/chat WebM on. CMake 3.28.3, Ninja, GCC 13.3, Ubuntu 24.04, Qt 6.4.2. Exactly 12 existing plugin conversion warnings match the recorded baseline; no new diagnostics ([comparison](build-warning-comparison.json), [inventory](build-warnings.txt)). [Configure](configure.log.gz), [full build](build.log.gz), [freshness build](build-freshness.log.gz), [binary manifest](fork-build-manifest.json).

The documented cache `~/.cache/mumble-sync-build/2026-10-21` was reused with `cmake --fresh` to bind it directly to the new source worktree. Cached SOCI/spdlog/rnnoise model sources were explicitly supplied through `FETCHCONTENT_SOURCE_DIR_*`; previous worktree aliases were not created or changed. Configuration follows AGENTS.md with SQLite and online tests on and MySQL/PostgreSQL service tests off. Build uses two jobs. Full test command:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/.cache/mumble-sync-build/2026-10-21 \
  --output-on-failure --output-junit <evidence>/ctest.xml -j 2
```

CTest: **42 passed, 3 failed, 1 previously disabled**, 246.82 seconds ([log](ctest.log), [JUnit](ctest.xml), [summary](ctest-summary.json)). File-transfer E2E passed in 238.98 seconds. Protocol, database, file-transfer manager and new IPC cases passed.

- `TestFileTransferEngine`: existing `selectedPinSurvivesCacheRemoval` empty-QList assertion.
- `TestChatWebM`: existing `audioPlaybackAdvancesClockAndResumes`, `loopsAfterEnd`, `resumeAtClipEndRestartsImmediately`, `pauseResumeKeepsFrameSchedule` audio-clock/EOF timing failures; other 35 cases passed.
- `TestServerResolver`: `simpleSrv` and `srvCustomPort` return zero records because the public DNS fixtures return NXDOMAIN ([direct queries](dns-diagnostics.log)); A/AAAA/CNAME cases passed.

Affected test and production sources are identical to fork master ([source comparison](baseline-source-comparison.json)). The engine/WebM signatures match both the [previous sync](../2026-10-21/README.md) and the [November 2 baseline/candidate run](../../performance/2026-11-02/test-setup.md). This run did not rebuild a separate complete baseline checkout. No assertions were removed, relaxed or disabled. OverlayTest was already disabled. Workflow YAML parsing, shell syntax and whitespace checks also passed.

## Official reference provenance

Official master was built from a clean, unmodified checkout at `08ac9b7ee87ad996eb7b4238bc57f160d0b01fce`: Debug, non-unity client/server, warnings-as-errors on, plugins/tests/LTO/overlay cross-compilation off. Build passed without compiler warnings. [Configure](ref-master-configure.log.gz), [build](ref-master-build.log.gz), [freshness](ref-master-freshness.log.gz), [manifest](ref-master-manifest.json).

The official release API still reports v1.5.939, published 2026-10-11, commit `dfbac8d3d9c967003e74b4ffc5610f7d45e8f666`. Its clean cached Qt 5 build with `BUILD_NUMBER=939` was reused only after verifying both binary hashes against its manifest ([verification](ref-stable-manifest.json)). Reference source/build caches are retained at `~/.cache/mumble-upstream-ref/<full-sha>`.

## Existing database incompatibility and remedy

Both fork and official master produce schema 11 with `meta_key`/`meta_value`. Stable schema 9 expects `keystring`/`value`; both downgrade attempts fail with `no such column: value` ([schema metadata](database-schemas.json)). The fork loads official master and stable databases/configs, and official master loads fork databases/configs, preserving stored SuperUser authentication and the Interop channel; SQLite integrity checks return `ok`.

This limitation predates the merge and is inherited from official master, not a new sync regression. A safe rollback restores a consistent pre-upgrade schema-9 snapshot with matching stable binary/configuration/TLS state; see the [rollback rehearsal](../../../hosting/historical-schema-rollback-rehearsal.md). Supporting current data on stable needs a separately reviewed export/down-migration covering identities/authentication, channels, groups, ACLs, bans and TLS identity. Merely renaming metadata columns or changing the schema number is insufficient. No unsafe automatic downgrade was added.

Windows/macOS execution (including the revised macOS IPC expectation and signing/packaging), static Linux CI, MySQL/PostgreSQL service tests, physical capture and media quality were not exercised locally. Published evidence excludes fixture databases, private keys, certificates and settings; logs omit the disposable password. The PR is not merged.
