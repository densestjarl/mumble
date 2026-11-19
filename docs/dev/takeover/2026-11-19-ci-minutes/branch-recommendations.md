# Stale fork branches — 2026-11-19

Only live origin heads unmerged by ancestry and idle for at least 14 full days are listed.
Recommendations are advisory; no branch or PR was deleted, merged or closed.
The selected branch is `t3/review-github-actions-minutes` (PR #63): keep until its takeover merges, then delete.

The older stability family shipped in squash-merged [PR #40](https://github.com/densestjarl/mumble/pull/40).
I checked ancestry against its source head `92968864814ac98a84546f051b31e538874b68f7`:
each branch below is either contained in that head, adds only validation records, or is the documented Windows fixture equivalent.
Archive unique evidence before deleting; existing evidence is indexed by the [PR #40 evidence release](https://github.com/densestjarl/mumble/releases/tag/pr40-evidence-2026-10-11).
The archives were not re-downloaded in this audit.

| Branch | Last commit (UTC) | Recommendation |
| --- | --- | --- |
| `sync/upstream-2026-10-28` | 2026-11-04 | Keep — PR #65 contains useful upstream work; update and verify it separately, preserving fork policy. |
| `slop-audit-2026-11-03` | 2026-11-03 | Keep — PR #69 is a small, independent cleanup; review and validate separately. |
| `perf/explicit-ui-outputs-20261102` | 2026-11-03 | Keep — PR #67 has a focused build optimization; validate its measurements separately. |
| `perf/video-packetizer-reuse-20261102` | 2026-11-03 | Keep — PR #66 has a bounded video optimization; validate its measurements separately. |
| `audit/slop-2026-10-27` | 2026-10-27 | Keep — PR #64 is a small, independent cleanup; review and validate separately. |
| `codex/pop-out-chat` | 2026-10-25 | Keep — protected checkout branch; PR #70 already replaces PR #56. Delete only after #70 merges and the checkout is released. |
| `codex/windows-blocker-evidence-80ae981` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-received-save-5eadce0-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-received-save-de8caa9-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/file-transfer-presented-identity` | 2026-10-11 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/windows-presented-sender-9b5b8e-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-presented-identity-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-automatic-save-migration-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-automatic-save-safety-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-atomic-save-sync-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-atomic-transfer-save-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-integrated-pacing-audio-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-rate-credit-precision-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-rate-credit-revised-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-audio-wizard-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-rate-credit-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/early-chunk-transfer-routing` | 2026-10-11 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/private-transfer-storage` | 2026-10-11 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/windows-early-routing-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-fatal-startup-corrected-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-private-storage-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/explicit-database-path` | 2026-10-11 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/file-transfer-accessible-actions` | 2026-10-11 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/macos-accessibility-runtime` | 2026-10-11 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/windows-fatal-startup-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-integrated-candidate-evidence` | 2026-10-11 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-database-path-evidence` | 2026-10-10 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-database-path-negative-evidence` | 2026-10-10 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-file-transfer-ui-evidence` | 2026-10-10 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/windows-qtlogger-evidence` | 2026-10-10 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/server-output-queue` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/windows-output-queue-evidence` | 2026-10-10 | Delete after archiving evidence — code shipped in #40; only additional validation records remain. |
| `codex/server-bandwidth-meter` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/windows-bandwidth-evidence` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/transfer-worker-shutdown` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/historical-schema-rollback` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/windows-shutdown-evidence` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/stability-foundation` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/stability-windows-fixture-fix` | 2026-10-10 | Delete after archiving evidence — the sole code patch matches d8e8812f9, already shipped in #40. |
| `codex/guest-state-recovery` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |
| `codex/guest-deployment-drill` | 2026-10-10 | Delete after archiving evidence — included in the source head of squash-merged #40. |

`takeover/pop-out-chat` / PR #70 is 13 days 23 hours old at the audit time (18:01 UTC); keep its existing replacement PR rather than duplicate it.
The nine November 16–19 branches were excluded because they received commits within the last three days.
Upstream release/backport branches were not treated as unfinished fork work.
