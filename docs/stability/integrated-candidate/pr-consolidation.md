# PR consolidation into #40

PR40 is the single review candidate for the work from PR30 onward. Every current head commit from PR30–39 and PR41–52 is an ancestor of the consolidated branch; no component code is discarded. The [machine-readable audit](pr-consolidation.json) records exact heads and the history checks. Closing a superseded draft preserves its commits, discussion, review and evidence. It does not approve the change or put it on master.

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

PR43 required a history reconciliation because its older fixture recalculated a legacy SHA1 identifier. The integration retains its TLS minimum and the newer authenticated server-provided identifier from PR45. PR52 contributes the qualified received-save trust and duplicate-save corrections. The non-document source tree matches tested source `5eadce0753b634147f97746e192ddef613019a02`.

PR29 is also an ancestor but remains open as required review context. The default branch has not moved. GitHub requires **one approving review**; PR40 currently has no approval and remains a draft while the documented correctness/acceptance work is unfinished. No self-approval or protection override is used.

## Actual remaining blockers

- GitHub merge eligibility: PR40 must leave draft state and receive one valid approving review. There is no merge conflict with the current master base. The branch protection API currently specifies no required status-check contexts; we still require fresh hosted build/package/security checks as our quality gate.
- Fresh automated qualification: all 41 enabled suites pass on available Mac, native Windows and Linux for the frozen runtime source. Prior integration 271b passed every hosted workflow and the combined security check. The new consolidated head needs its own hosted checks; older results do not substitute.
- Known correctness work for stability acceptance: targeted server relay and recipient receipts/rate refusal, downstream backpressure and slow receiver continuity, sender first-use authorization and receiving connection/request-token lifecycle. See the [review work order](review-work-order.md) and [trust work order](trust-follow-up-work-order.md). These are not completed by a green suite.
- Release/paid-hosting acceptance: installed GUI/device/permission matrix, shipping signatures/notarization, supported OS and filesystem behavior, endurance, exact-final-package deployment/restore and provider/density/support/egress measurements. Available Mac local speaker/microphone checks pass; unavailable hardware is not marked passed.

Merge eligibility and stable paid-release acceptance are separate decisions. This consolidation preserves and advances the fixes; it does not certify a stable release or profitable hosting.
