# Upstream sync evidence — 2026-10-28

**COMPAT BREAK:** official stable v1.5.939 cannot open the fork's schema-11 database. This is an existing limitation inherited from official master, which reproduces the same failure. All four mixed client/server protocol and GUI sessions pass.

## Revisions and changes

- Authorized fork destination: `densestjarl/mumble`. The legacy `gavmac11/mumble` URL redirects there; the user explicitly authorized continuing at the new destination.
- Fork master: `7aa4335fb3466ed98e80331c91c574e784f454f0`; official master: `162f42805ce095c37ab57f1d81ba92cdb31bbac5`.
- Initial ancestry: 172 ahead, 18 behind. No open sync PR or competing active sync run existed. The merge branch has no missing upstream commits.
- Merge `de8bfeb4e94765af48daa859181e9b5ac6982b1e` has the fork and upstream masters as parents. Merged range: `c7a8993caa4b657c93d3c2456e6cf2eaaafe9e85..162f42805ce095c37ab57f1d81ba92cdb31bbac5` (18 commits; [list](merge-range.txt)).
- There were no conflicts or manual merge resolutions. Incoming changes add the Remnant II positional-audio plugin, update Teardown and Grounded plugins, register the installer plugin, and update translations. Fork video/screen sharing, chat media, file transfer, AI tooling and contributor policy are preserved; [preservation check](fork-feature-preservation.txt).
- The compatibility harness still expected a passphrase prompt removed by the fork's earlier automatic device-key identity work. Its initial positive fixture failed only on that obsolete assertion ([result](harness-before-update/result.json)). The harness now rejects unexpected identity prompts and verifies both file capability and delivery receipts on the wire. The corrected full positive fixture passes. Application code was not changed by this correction.

## Compatibility matrix

| Check | Result | Evidence |
|---|---|---|
| Protocol against master | PASS | [descriptor and ID audit](protocol-master.json) |
| Protocol against stable v1.5.939 | PASS | [descriptor and ID audit](protocol-stable.json) |
| PROTOCOL.md / version negotiation | PASS | [static review](protocol-review.md) |
| Official master client → fork server | PASS, 125.80 s | [result](interop/master-client/result.json), [screenshot](interop/master-client/soak-complete.png) |
| Fork client → official master server | PASS, 125.75 s | [result](interop/master-server/result.json), [screenshot](interop/master-server/soak-complete.png) |
| Official v1.5.939 client → fork server | PASS, 125.64 s | [result](interop/stable-client/result.json), [screenshot](interop/stable-client/soak-complete.png) |
| Fork client → official v1.5.939 server | PASS, 125.74 s | [result](interop/stable-server/result.json), [screenshot](interop/stable-server/soak-complete.png) |
| Fork-to-fork automatic identity and controls | PASS, 125.36 s | [result](interop/fork-positive/result.json), [screenshot](interop/fork-positive/soak-complete.png) |
| Official master DB/config → fork | PASS | [result](database/master-to-fork/result.json), [log](database/master-to-fork/server.log.gz) |
| Fork DB/config → official master | PASS | [result](database/fork-to-master/result.json), [log](database/fork-to-master/server.log.gz) |
| Official stable DB/config → fork | PASS | [result](database/stable-to-fork/result.json), [log](database/stable-to-fork/server.log.gz) |
| Fork DB/config → official stable | **FAIL: schema 11 → 9** | [result](database/fork-to-stable/result.json), [log](database/fork-to-stable/server.log.gz) |
| Official master DB → official stable (control) | **FAIL: schema 11 → 9** | [result](database/master-to-stable/result.json), [log](database/master-to-stable/server.log.gz) |

Each GUI fixture uses a native binary under Xvfb, joins Interop, displays GuiInterop and SuperUser, exchanges text in both directions, moves to Root and back, and receives another text after at least 125 seconds. A TLS observer checks the server's user/channel state and reports zero errors. Final screenshots were inspected. Fork clients keep video/file controls disabled on official servers; the fork-to-fork fixture enables both and announces file capabilities without a setup dialog. Client/server logs and observer records are compressed beside each result.

All measured sessions remained alive and usable. Intentional SIGTERM cleanup follows the measurement. Both stable mixed fixtures required SIGKILL for server shutdown after the timeout (one fork server, one official stable server); master mixed fixtures and the corrected positive fixture did not. This is a cleanup limitation, separate from connection liveness. It resembles the previously documented intermittent shutdown hang, but this run did not collect a new stack trace or establish its exact cause. Clean shutdown is therefore not fully established.

## Build and tests

CMake 3.28.3, Ninja, GCC 13.3, Ubuntu 24.04; Qt 6.4.2 for fork/master and Qt 5.15.13 for stable. Full Debug client/server, plugins, screen/file sharing, chat WebM and test build passed with unity/LTO off and project warnings-as-errors on. The 12 plugin conversion warnings match the prior recorded baseline exactly: [comparison](build-warning-comparison.json), [inventory](build-warnings.txt). No new compiler diagnostics. SOCI also emits its existing configuration warning. See [configuration](configure.log.gz), [build](build.log.gz), [no-work freshness check](build-freshness.log.gz).

The existing build directory `~/.cache/mumble-sync-build/2026-10-21` was reused. Its former source path, which no longer existed, temporarily symlinked to this run's fresh `2026-10-28` worktree. The temporary alias is removed with the worktree; the build cache stays available. Every affected source was compiled from the new checkout.

```sh
cmake -S <fresh-worktree-source-alias> -B ~/.cache/mumble-sync-build/2026-10-21 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=cmake/mumble_dependency_provider.cmake \
  -DBoost_NO_BOOST_CMAKE=ON -DCMAKE_UNITY_BUILD=OFF \
  -Dclient=ON -Dserver=ON -Dtests=ON -Dwarnings-as-errors=ON \
  -Ddatabase-sqlite-tests=ON -Ddatabase-mysql-tests=OFF \
  -Ddatabase-postgresql-tests=OFF -Doverlay-xcompile=OFF -Dlto=OFF -Donline-tests=ON
cmake --build ~/.cache/mumble-sync-build/2026-10-21 -j 3
QT_QPA_PLATFORM=offscreen CTEST_PARALLEL_LEVEL=2 \
  .github/workflows/test.sh ~/.cache/mumble-sync-build/2026-10-21 Debug
```

Full CTest: **42 passed, 3 failed, 1 already disabled**, 286.04 seconds. File-transfer E2E passed in 278.22 seconds; server database, protocol, main-window file transfer and manager suites passed. [Full log](ctest.log), [JUnit](ctest.xml).

- `TestFileTransferEngine` aborts in `selectedPinSurvivesCacheRemoval` on an empty QList assertion, matching the prior baseline.
- `TestChatWebM` fails the same four audio-clock/EOF timing checks recorded previously: `audioPlaybackAdvancesClockAndResumes`, `loopsAfterEnd`, `resumeAtClipEndRestartsImmediately`, `pauseResumeKeepsFrameSchedule`. Its other 35 cases pass, including device-independent mixer checks.
- The newly enabled online `TestServerResolver` fails `simpleSrv` and `srvCustomPort`: both public DNS fixture names return NXDOMAIN ([DNS evidence](dns-diagnostics.log)). Its A/AAAA/CNAME cases pass.

All non-translation production/test sources under `src/` are identical to current `origin/master`; [source comparison](baseline-source-comparison.json). The prior run already recorded the engine and WebM failures ([prior evidence](../2026-10-21/README.md)). This run did not rebuild a separate complete baseline checkout. The unchanged sources and matching failure signatures distinguish existing failures from the merge. No test assertions were disabled or weakened. OverlayTest was already disabled.

## Reference provenance

Official master was built from a clean source checkout at the exact SHA, with client/server, Debug, non-unity, warnings-as-errors, plugins/tests off and LTO/overlay cross-compilation off. Its initial one-job build was deliberately stopped and resumed with three jobs after the fork build finished; the final build succeeded without warnings. [Configuration](reference-configure.log.gz), [build](reference-build.log.gz).

The latest stable release returned by the official release API was v1.5.939 (2026-10-11), commit `dfbac8d3d9c967003e74b4ffc5610f7d45e8f666`. Its cached clean build uses BUILD_NUMBER=939, and both current binary hashes match its existing manifest. Reference source/build caches are retained at `~/.cache/mumble-upstream-ref/<sha>`. [Manifests and binary hashes](build-manifest.json).

## Database failure cause and remedy

Schema 11 uses `meta_key`/`meta_value`; stable schema 9 expects `keystring`/`value`. Fork-to-stable and the independent official-master-to-stable control both fail with `no such column: value`. See [schema metadata](database-schemas.json). Database sources are unchanged by this merge. This is existing fork behavior inherited from official master, not a sync regression.

Rollback requires a consistent pre-upgrade schema-9 snapshot with its matching stable binary/configuration/TLS state, as described in the [rollback rehearsal](../../../hosting/historical-schema-rollback-rehearsal.md). Supporting current data in stable would require a separately reviewed export/down-migration covering authentication, channels, groups, ACLs, bans and TLS identity. Changing only a schema number is unsafe; no automatic downgrade was added.

Windows/macOS builds, MySQL/PostgreSQL service tests, physical audio/video capture and media quality were not exercised. Online tests were exercised, with the DNS limitation above. Published logs omit the disposable fixture password; no private keys, certificates, settings or databases are included. The PR is not merged.
