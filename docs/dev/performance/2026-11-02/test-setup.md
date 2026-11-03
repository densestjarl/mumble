Relay-focused file-transfer E2E cases now create fresh device identities using the same API as normal client onboarding. They retain real key generation, identity encryption, signing, trust-store checks, handshakes, and encrypted transfers, while avoiding repeated passphrase KDF setup that their assertions do not exercise.

The basic `endToEndTransfer` still creates both passphrase-protected identities. `endToEndPasswordTransfer` still tests the file-password KDF and password-protected transfer. `TestFTIdentity` still tests passphrase creation, unlock, wrong passwords, rewrapping, signing/context separation, and device identity lifecycle. Transfer sizes (including 96 MiB and two 48 MiB cases), rate budgets, capability gates, targeted delivery, retries, delivery receipts, and assertions are unchanged. Every client still gets a fresh real keypair in its private SQLite database. No production code or cryptographic cost parameters changed.

Same Linux non-unity Debug client/server/plugins/tests configuration, two CTest jobs, offscreen Qt, cached dependency sources, SQLite enabled, online/MySQL/PostgreSQL tests omitted:

| Measurement | Initial baseline | Candidate |
| --- | ---: | ---: |
| File-transfer E2E suite | 276.61 s | 238.74 s |
| Full CTest wall time | 277.16 s | 239.22 s |
| Passphrase-identity creations in E2E | 12 | 2 |
| Fresh identity keypairs in E2E | 12 | 12 |
| Passed / baseline-failed / disabled suites | 42 / 2 / 1 | 42 / 2 / 1 |

The initial-baseline reduction is **13.7%** for E2E and full CTest. Five pre-change E2E observations in this session ranged from 270.16 to 276.61 seconds; the single candidate observation, 238.74 seconds, is well outside that range. The other experiments changed client-only code or its build graph, not the standalone E2E fixture, server, transfer volumes, or crypto settings. [Times and controls](test-setup-times.json), [environment](test-setup-environment.json), [baseline log](test-setup-baseline-ctest.log.gz), [candidate log](test-setup-ctest.log.gz).

The full build passed in 289.30 seconds after restoring the original build graph; that is a validation rebuild, not a clean-build speedup. The [build log](test-setup-build.log.gz) retains the result. The first clean baseline build took 572.40 seconds. TestChatWebM's audio-clock/timing failures and TestFileTransferEngine's selectedPinSurvivesCacheRemoval QList assertion reproduce before and after. No failing tests or meaningful assertions were removed or disabled. Windows DPAPI execution and other platform timings remain for CI.

Reproduce with the documented Debug configuration in AGENTS.md, then:

```sh
cmake --build build -j 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build -j 2 --output-on-failure \
  --output-junit ctest.xml
```

The `TestFileTransferE2E` JUnit testcase contains the suite time; CTest reports the full wall time. Compare the same configuration and machine. Do not compare the temporary reconfiguration rebuild with the initial clean build. This change is independent of the packetizer and UI-generation performance changes.
