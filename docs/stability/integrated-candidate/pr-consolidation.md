# PR consolidation into #40

PR #40 contains every component head from PR30–39 and PR41–52. Closed component PRs retain their review history; integration merges did not merge into master. The exact historical ancestry audit is retained in the [evidence archive](https://github.com/gavmac11/mumble/releases/tag/pr40-evidence-2026-10-11), at `docs/stability/integrated-candidate/pr-consolidation.json`.

| PR | Change | Disposition |
| --- | --- | --- |
| [#30](https://github.com/gavmac11/mumble/pull/30) | FIX/CI: Establish native stability regression foundation | Superseded by #40 |
| [#31](https://github.com/gavmac11/mumble/pull/31) | FEAT(hosting): Add pinned private guest deployment rehearsal | Superseded by #40 |
| [#32](https://github.com/gavmac11/mumble/pull/32) | FEAT(hosting): Add encrypted private guest state recovery rehearsal | Superseded by #40 |
| [#33](https://github.com/gavmac11/mumble/pull/33) | Cancel and join busy file-transfer workers before teardown | Superseded by #40 |
| [#34](https://github.com/gavmac11/mumble/pull/34) | Add pinned historical encrypted recovery and reboot resume drill | Superseded by #40 |
| [#35](https://github.com/gavmac11/mumble/pull/35) | Fix full-width server media bandwidth accounting | Superseded by #40 |
| [#36](https://github.com/gavmac11/mumble/pull/36) | Bound server output retained by stalled TCP receivers | Superseded by #40 |
| [#37](https://github.com/gavmac11/mumble/pull/37) | Fix Mac accessibility crashes and late Qt logging teardown | Superseded by #40 |
| [#38](https://github.com/gavmac11/mumble/pull/38) | Preserve safety QR payloads and add keyboard file actions | Superseded by #40 |
| [#39](https://github.com/gavmac11/mumble/pull/39) | Stop silent fallback from explicit client database paths | Superseded by #40 |
| [#41](https://github.com/gavmac11/mumble/pull/41) | Show configured database errors and fix fatal teardown | Merged into the integration branch |
| [#42](https://github.com/gavmac11/mumble/pull/42) | Keep received plaintext in private random temporary storage | Superseded by #40 |
| [#43](https://github.com/gavmac11/mumble/pull/43) | Require TLS 1.2 for historical recovery readiness | Merged into the integration branch |
| [#44](https://github.com/gavmac11/mumble/pull/44) | Isolate early chunks and receive idle progress by transfer | Superseded by #40 |
| [#45](https://github.com/gavmac11/mumble/pull/45) | Use server-provided ban identifiers in recovery fixtures | Merged into the integration branch |
| [#46](https://github.com/gavmac11/mumble/pull/46) | Enforce a shared file-transfer sending rate | Merged into the integration branch |
| [#47](https://github.com/gavmac11/mumble/pull/47) | Handle unavailable devices in the audio wizard | Merged into the integration branch |
| [#48](https://github.com/gavmac11/mumble/pull/48) | Restore both CodeQL comparison configurations | Merged into the integration branch |
| [#49](https://github.com/gavmac11/mumble/pull/49) | Preserve files during failed and confirmed transfer saves | Merged into the integration branch |
| [#50](https://github.com/gavmac11/mumble/pull/50) | FIX: separate manual save history and refuse unsafe automatic filenames | Merged into the integration branch |
| [#51](https://github.com/gavmac11/mumble/pull/51) | FIX: Bind file transfer trust to the actual peer identity | Superseded by #40 |
| [#52](https://github.com/gavmac11/mumble/pull/52) | Recheck received sender identity before saving files | Merged into the integration branch |


PR29 is also included and remains open as review context. PR40 requires a valid human approving review after the requested corrections. Consolidation and passing automated checks do not qualify a stable or paid hosting release.
