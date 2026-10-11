This review was read-only. I used only Read, Grep and Glob on the snapshot and did not build, run or hash anything. All build, test and CI results below are evidence you supplied. Plan mode asked for a plan file and ExitPlanMode, but you told me not to write files or request other tools, so this reply is the whole output.

## New consolidation Must/Should

**Must: none.** No earlier correction was lost and the merge introduced no source regression. On this path, the merged `FileTransferManager.cpp` keeps all of these:
- the PR33 worker join (`:41-47`);
- the PR46 shared send rate (`:549`);
- PR51's checks against the identity captured on the transfer (`:451-465`, `:308-314`);
- PR52's recheck before saving and its duplicate-save guard (`:363-385`, `:68-106`).

Auto-save still works because the Ready entry is stored (`:98`) before `transferUpdated` is emitted (`:105`). That signal reaches `MainWindow.cpp:255` on the same thread, so `saveTransferAs` at `MainWindow.cpp:4841` finds the entry. A Ready that carries an error is not auto-saved again (`MainWindow.cpp:4835`), so a failed save can't loop.

One test expectation changed on purpose: `TestFileTransferManager.cpp:223` now expects `peerBlocked` only for a changed key, not for a removed pin, missing name or missing key. This matches PR52's split between "missing" and "changed" errors and is not a lost fix.

**Should 1 (document): `pr-consolidation.md:34-39` leaves out a correctness item I confirmed in the source.** The section is titled "Actual remaining blockers", but it drops open work that `review-work-order.md:16-17` still lists.
- **Lost SSH access on a non-default port.** `deploy_guest.py:251,210` lets you choose the SSH port. But `historical_recovery.py:285-290` installs an nftables table that drops everything by default and only accepts TCP port 22.
- **Trigger:** deploy with `--ssh-port 2222`, then run a historical restore. In nftables, a drop in any base chain on the input hook wins, so the deploy table's accept rule doesn't help. Inbound SSH on 2222 is dropped and the operator loses access to the guest.
- **Also missing:** the crash-loop negative control for readiness and the packaging-without-Cocoa-repair negative (`review-work-order.md:16-17`).
- **Fix:** add these to the blockers list.

**Should 2 (test gap, existed before this merge, not introduced by it): PR43 and PR45 behaviour is in the source but nothing guards it.**
- `test_recovery_fixture.py` is not run by `hosting-tools.yml:28-31` or by CTest. Your "41 suites" and the earlier 271b hosted run therefore did not run the PR45 ban-identifier tests.
- No test checks `minimum_version`. `test_historical_recovery.py:134-148` replaces the SSL context with a mock but never asserts the TLS minimum.
- A future history reconciliation could revert either fix and every check would still pass. I can't tell from this snapshot whether PR45's head ever wired the test in, so I'm not calling this a merge loss.

**Nits (not new):**
- `FileTransferManager.h:165` still says "until abort". The earlier review already noted this.
- The new status in `trust-follow-up-work-order.md:9` ("retains useful terminal failure") doesn't repeat the review's test gaps. Nothing tests a save that fails after the pin was revoked, and three rows have no negative run.
- These link targets are not in the snapshot, so I couldn't check them: `pr-consolidation.json`, `revision-5eadce0/README.md`, `native-mac-local-audio.md`, the `opus-review.md` linked from `review-work-order.md:3`, and `presented-identity/revision-9b5b8e/README.md`. Please confirm they resolve at b4a3.

**Are unresolved findings improperly marked accepted? No.**
- The documents keep the stable release and merge eligibility separate.
- The changed trust row names what remains: MainWindow flows and revocation during a write.
- `review-work-order.md` rows for PR42 and PR44 still say "First" with no completion claim. That is stale, but it errs on the conservative side.

## PR43 reconciliation: no TLS or ban regression
- **TLS:** `historical_recovery.py:189` sets TLS 1.2 as the minimum before the socket is wrapped (`:198`). That check gates readiness (`:311`) before the "ready" marker is written (`:313`). It applies only to the local readiness probe; the server's own TLS configuration is unchanged.
- **Ban identifier:**
  - `recovery_fixture.py:56-67` reads the hash the server supplies in the connected user's own UserState (field 15).
  - It requires 40 lowercase hex characters and ignores other users' entries.
  - The seed and recover steps compare that same value (`:171`, `:211`).
  - Nothing under `scripts/hosting` calls SHA1; the only `sha1(` matches are unrelated C++.
  - The record key is still named `banned_certificate_sha1`, which keeps existing records readable.
- **Gap:** the only weakness is the missing test guard in Should 2.

## Known correctness items that prevent calling this stable
I confirmed these in the current source:
- **First-use prompts are tracked per session only.**
  - Pending prompts are stored by session (`FileTransferManager.cpp:160-162`).
  - Their result is sent back to the engine by session alone (`:117-121`, `:402-403`), with no transfer, kind or engine request token.
  - The user's name is read when the prompt appears (`:162`), not together with the key.
- **Incoming messages aren't tied to their connection.** `handleControlMessage` and `handleDataMessage` (`:224-252`) queue messages without the connection generation; only outgoing engine events are filtered (`:184-196`).
- **Disabled feature resets repeatedly.** While file transfer is disabled, every `refreshPinCache` calls `applyEngineConfig` (`:510`), which calls `disconnectCleanup` (`:556-558`) and bumps the generation each time.
- **Historical restore drops SSH on a non-default port** (Should 1).
- **Revocation during an approved write** is not guaranteed by design (`validation.json:60`).

These remain documented, but I did not re-check them in source this time: server relay and recipient receipts or rate refusal, downstream backpressure and slow-receiver continuity, and atomic name/key lookup with the re-verification and case rules.

## Checks still unavailable
- **Hosted CI:** fresh checks on the combined head are still running. The earlier 271b results don't count for this head.
- **Installed client:**
  - MainWindow manual and automatic save flows, including a manual save during an auto-save;
  - network voice, camera/screen sharing and device hot-swap;
  - Windows 11 and Windows capture/focus;
  - physical Ubuntu devices.

  Mac speaker and microphone are user-confirmed only.
- **Real-server tests:** concurrent transfers to multiple recipients.
- **Release gates:**
  - signed and notarized shipping packages;
  - endurance runs;
  - deploy, restore and reboot using the exact final package;
  - provider, density and egress measurements.
- **Review:** PR29 human review.
- **GitHub eligibility (a separate gate):** PR40 has to leave draft and get one valid approval from someone else. None of the above should be worked around by bypassing protection, self-approving, removing scan history or marking missing hardware as passed.
