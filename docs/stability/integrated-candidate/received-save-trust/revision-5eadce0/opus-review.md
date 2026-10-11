**Your Should is fixed, and I found no new Must or Should.** I read the snapshot read-only and didn't build or run anything. The pass/fail results below come from your logs, not from me. I also didn't hash the files; I'm relying on your statement that they were checked against Git at `5eadce07…`. Plan mode asks for a plan file and ExitPlanMode, but neither tool is available here, so this review is the whole output.

## Is the earlier Should fixed? Yes

**The duplicate save can no longer cause a rejection.**
- `saveTransferAs` now ignores a request while a save for that transfer is still running (`FileTransferManager.cpp:366-367`).
- That check comes before the trust recheck, so a second click can't trigger `rejectReceive` behind a save that was already approved.
- The transfer is only marked as saving after the recheck passes (`:378`). Recheck failures still reject as before (`:374-376`).

**The marker can't get stuck or be cleared too early.**
- It's cleared on any incoming Ready, Saved, Failed or Aborted event (`:69-73`). These are exactly the events the engine can send after a queued save:
  - a successful save sends Saved (`FileTransferEngine.cpp:1450-1463`);
  - every failure sends Ready with an error (`:1466-1478`), including an interrupted copy, which also takes the failure path (`:1242-1243`);
  - the save only does nothing when the job is already gone (`:1222-1224`). A job only disappears after Saved, Failed or Aborted, and that event is queued to the GUI in order. So either the save was never marked or the marker gets cleared.
- An old Ready can't clear the marker early. The first Ready is handled before a save is possible, and a Ready with an error is the reply to the previous save.
- Disconnect clears it (`:496`). Events from the old connection are dropped, so they can't touch the new state.

**A failed save still goes through the trust recheck.** The Ready-with-error clears the marker and then reaches the Ready branch (`:91-98`). There it either goes back on the ready list or is rejected with the "Cannot confirm" or "changed" message. So the guard doesn't let a retry skip the recheck.

**Saved and Failed are no longer lost for rejected transfers.**
- A Saved for a rejected transfer is now reported as Saved and the stored rejection is removed (`:76-81`).
- A Failed or Aborted shows the stored trust failure and removes it (`:83-88`).
- The stored value is copied before it's removed, and `transferUpdated` is emitted afterwards, so nothing breaks if the GUI calls back into the manager during the update.
- With the guard in place, I can't find a way for a rejected transfer to receive Saved. A rejection from `saveTransferAs` needs no save running, and a rejection from the Ready branch happens just after the marker was cleared. That branch is now a safety net that never runs in practice, which is fine.

**The test checks the right thing.**
- The duplicate-save row (`TestFileTransferManager.cpp:407-425`) blocks the worker first, so the approved save is definitely still queued when the pin is removed and the second save is attempted.
- It then checks that the file content is correct, the last update is Saved, nothing was blocked, both lists are empty, and `owned-duplicate.bin` doesn't exist (`:433-451`).
- Your negative log shows the old source ending in Failed (state 7) instead of Saved (state 6). That proves the bug existed and that this row catches it.

## Must-fix
None.

## Should-fix
- **No new Should from this change.**
- **Still open from earlier lists:**
  - sender first-use lifecycle;
  - receive connection and request tokens;
  - atomic name/key lookup;
  - the re-verification flow;
  - re-entry when the feature is disabled;
  - name case-sensitivity;
  - relay routing and receipt;
  - hardware.

  This change doesn't settle any of them.

## Behaviour changes (acceptable, not regressions)
- **A second save request is dropped without any message.**
  - Example: auto-save is running and the user picks a path in the card's Save dialog (`MainWindow.cpp:4877-4895`). The engine still has no Saving state, so the card looks clickable.
  - Before this change, the second request ran if the first save failed. Now the user sees the failure and has to click again.
  - No data is lost or overwritten, and failing visibly is arguably better.
- **A trust change during an approved save only shows up if that save fails.** If it succeeds, the card says Saved and doesn't mention the later trust change. That's consistent with treating the approved write as legitimate.
- **Engine error text can be hidden.** A real engine Failed for a rejected transfer now shows the stored trust failure instead of the engine's message. The job is gone either way.
- **A header comment is now slightly out of date.** `FileTransferManager.h:168` still says "until abort"; cleanup now also happens on Failed and Saved.

## Test gaps (not blocking)
- **The negative log was not run with the final test file.** It points to line 431, but the Saved check in this snapshot is at `:438`. The final test also uses `m_savesInFlight`, which doesn't exist in the old source. So the negative ran a modified version of the test, and only the duplicate-save row was run.
- **The no-trust-store, removed-disconnect and new error-text rows have no negative run.** Nothing shows they fail on the old source.
- **No test covers the most important combination:** a save is running, the pin is removed, then the save *fails*. The expected result is a Ready-with-error, then the trust recheck, then Failed with "Cannot confirm", with both lists cleared.
- **The two new rejected-transfer branches have no tests.** These are Saved arriving for a rejected transfer, which seems unreachable now, and a real engine Failed for a rejected transfer.
- **The removed-disconnect row passes its empty-list checks automatically,** because `disconnectCleanup` clears the lists anyway. What it really tests is that the last card state is still Failed, which is the useful part.
- **Not covered at all:**
  - the `MainWindow` auto-save and card-click paths, including a manual save during auto-save;
  - disconnecting while an approved save is still copying. In that case the Saved event belongs to the old connection and is dropped, so the card can stay at Ready. That behaviour already existed before this change and isn't new.

## Acceptance gaps
- Full Mac/Windows/Linux qualification on this exact source is still pending.
- The 45-case manager run is your macOS result, which I haven't seen run.
- This review doesn't claim:
  - real TLS, network or `MainWindow` GUI behaviour;
  - revocation after a save is queued or mid-copy. A write that was approved before revocation is still allowed to complete.
- The unrelated trust, relay and hardware work listed above is still required.
