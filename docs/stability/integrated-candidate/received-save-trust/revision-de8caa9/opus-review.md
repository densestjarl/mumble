**No Must-fix issue remains.** The patch fixes the earlier Should item about rechecking trust on manual save, and partly fixes the one about terminal diagnostics. I found no regression that weakens security. I did find one new Should: in a narrow race, the manager hides a real `Saved` event. Details are below.

I reviewed the snapshot read-only and didn't build, run or reproduce anything. I didn't hash the files myself; I'm relying on your statement that they were compared with `git show` of `de8caa9e…`. Plan mode asks for a plan file and ExitPlanMode, but those tools aren't available in this session, so this review is the whole output.

## What the patch fixes

**Manual-save recheck: fixed.**
- `saveTransferAs` (`FileTransferManager.cpp:351-369`) only queues a save for a transfer that was accepted at Ready (`m_readyReceived`).
- Just before queueing, it rechecks the saved pin for the name and key captured at Ready, using `trustStateForTransfer`.
- Anything other than Pinned or Verified goes to `rejectReceive`. That includes a removed pin, a changed key, no trust store, and a database lookup failure, because `lookup` returning false gives NewPeer (`:443-444`).
- Nothing processes events between the check and the queued call.
- Auto-save goes through the same gate. The manager inserts into `m_readyReceived` before it emits (`:86`, `:93`), so `MainWindow.cpp:4835-4842` finds the entry. Auto-save also still requires Verified (`FTSavePolicy.cpp:38`).
- A failed save comes back as Ready with an error (`FileTransferEngine.cpp:1466-1478`). The manager rechecks trust on that event too and either re-registers the transfer or rejects it. Retrying after a failure therefore can't skip the recheck.
- The negative log shows the old source writing the file in the `removed` and `changed` rows, and the fixed source rejecting them. That proves the attack and the fix.

**Terminal diagnostics: mostly fixed.**
- A rejection is stored in `m_rejectedReceived`.
- When the engine's cleanup emits Aborted (`FileTransferEngine.cpp:1214`), the manager emits the stored Failed state again instead (`:69-77`). Your fixture waits for the worker and the GUI forwarding before asserting that the last update is Failed, so it tests this ordering properly.
- A missing pin no longer emits `peerBlocked`. Only Changed does (`:347`, and the updated test at `:127`). That's the right split.

## Stale events and disconnect: no regression

- `disconnectCleanup` clears both new maps (`:478-479`).
- If a rejection's abort was queued before the disconnect, the worker processes it under the old engine generation. Its Aborted event is dropped by `forwardEngineEvent` (`:179`), and `abortAll` then finds no job, so the card stays Failed.
- An Aborted from `abortAll` for a transfer that wasn't rejected removes it from `m_readyReceived`.
- Old-generation Ready events can't reach the save gate.

## Behaviour changes (acceptable, worth knowing)

- **Fails closed with no trust store.** The old Ready check was wrapped in `if (m_trustStore && …)`, so with no database every incoming Ready went through. Now every incoming Ready is rejected with the "Cannot confirm" message. That's correct, but it's new behaviour when `Global::get().db` is null.
- **Downgrade from Verified to Pinned is still allowed for manual save.** This is by design, since the gate accepts Pinned.

## Should-fix

1. **New: a rejection can hide a real `Saved` event and leave the card saying Failed.**
   - The engine has no "Saving" state, so the card stays Ready while an authorized save is still copying.
   - Scenario: a save is authorized and queued (manual or auto-save). The pin is then removed or changed. A second save attempt arrives for the same transfer, either a second click or auto-save plus a manual click.
   - That second attempt calls `rejectReceive`: the card shows Failed and an abort is queued behind the first save.
   - The worker finishes the first save, emits `Saved` and cleans up (`:1450-1463`). The abort then finds no job and emits nothing (`:1211-1216`).
   - Back in the manager, `Saved` hits the rejected entry, isn't Aborted, and is silently dropped (`:77`). The file is on disk, but the user is told the transfer failed. The `m_rejectedReceived` entry also leaks until the next disconnect.
   - This isn't the "revocation after queueing" case you already excluded. That write was authorized and its outcome is allowed. The problem here is that its outcome is misreported.
   - Fix: when a rejected entry receives `Saved`, either report the save truthfully (for example Saved with a note that trust changed afterwards) or keep Failed with an error saying a save already completed at the given path. In both cases remove the entry. Also clean up on Failed, not only on Aborted.
   - A simpler mitigation: keep a set of transfers with a save in flight and drop or defer a second `saveTransferAs` while one is queued.
2. **Still open from earlier lists:**
   - sender first-use lifecycle;
   - receive connection and request tokens;
   - atomic name/key lookup;
   - the re-verification flow;
   - re-entry when the feature is disabled;
   - name case-sensitivity;
   - relay routing and receipt;
   - hardware.

   This review doesn't qualify any of them.

## Test notes and gaps

- The negative run only covered `verified`, `pinned`, `removed` and `changed`. The `storage-unavailable` and `save-retry` rows were never run against the old source, so it's not shown that they fail without the fix.
- The `storage-unavailable` row only asserts that the error is non-empty. It doesn't check that the "Cannot confirm" message was used or that `peerBlocked` stayed silent. No row asserts `peerBlocked` for `changed` on the save path.
- No test covers:
  - the race in Should 1;
  - a disconnect between a rejection and the worker's abort;
  - a trust store that is absent rather than closed;
  - the `MainWindow` auto-save or card-click path.

## Acceptance gaps

- The full native-suite qualification on the exact source is still pending. After that come Windows/Linux qualification and real TLS, network and `MainWindow` GUI testing, which you aren't claiming.
- Not guaranteed, as you stated: revocation after a save is queued, or mid-copy.
- Should 1 above, plus the separately tracked open work listed in Should 2.
