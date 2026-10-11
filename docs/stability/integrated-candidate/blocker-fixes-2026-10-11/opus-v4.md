I couldn't save this as a plan file because this session only has read-only tools (no Write or ExitPlanMode), so the full review is below. I read the snapshot and ran nothing, as you asked.

# Review: relay admission fairness patch (snapshot 185c0917)

## Must fix
**None.** I traced reentrancy, fairness, byte accounting, cancellation, own-versus-peer error routing, reset prompt scoping and restore ordering through the source, and found nothing that justifies a Must.

## Regressions: which were present, and what the patch does

1. **Starvation of the second outgoing job: present before the patch, fixed by it.**
   - **Before:** after a chunk was dispatched in acknowledgement mode, `paceSends` hit `break` with the job still at the front of the queue. Each acceptance restarts the timer at 0 ms (`FileTransferEngine.cpp:1310-1311`), and each pass ends by setting it to 50 ms (`:1160`).
   - **Effect:** when the server answers in under 50 ms, every pass starts with the large job, which is ready again. The rotation branch (`:1122-1128`) is never reached, so the small job sends nothing. Its stall timer then fails it after `receiveIdleTimeoutMSecs` (60 s, set at `FileTransferEngine.h:101`). That matches the 96 MiB / 3 MiB report.
   - **After:** with `&& !job->awaitingRelay` (`:1144-1145`), the dispatched job moves to the back of the queue (`:1149-1153`). Each job keeps one chunk in flight and they alternate.
   - **Local backpressure:** when the connection queue refuses a chunk, that job stays at the front and is retried first. The queue is shared across the connection (`ServerHandler.cpp:837-842`), so this is not unfair.
   - **Rate limiting:** the client paces at 90% of the server rate using one shared credit pool (`FileTransferManager.cpp:819-825`), which keeps server refusals rare.

2. **A synchronous acknowledgement was dropped: present before, fixed.**
   - **Before:** `awaitingRelay` was set only after the transport callback returned. A `relayResponse` arriving inside the callback was ignored (`:1284`) and the job hung.
   - **After:** the flag and the credit charge are set before the callback (`:1200-1207`). If the chunk is refused locally, both are undone (`:1208-1212`).
   - **Byte counting:** progress is only counted in `relayResponse` (`:1288`), or in non-acknowledgement mode after a state check (`:1214-1216`). Nothing is counted twice.
   - **Cancellation:** a job aborted during the callback is dropped by the check at `:1214`. `paceSends` does not re-queue it (`:1152`). A late response for it finds no job and is ignored.
   - There is only one caller of `sendNextChunk` (`:1143`).

3. **Null pointer in `abortPeer`: present before, fixed.** `m_sendJobs.value(id)` returns null once an earlier abort has removed that job (`:1615-1617`). The inner loop exits right after `abortTransfer`, so the cleared `peers` list is never read again.

4. **Restore with a missing or non-integer SSH port: present before, fixed.**
   - **Before:** the failure was already raised before any change to the guest, but as a `KeyError` or `TypeError`.
   - **After:** it is a `ValueError`, raised before `verify_ssh_port` and before `fresh_or_resumable`, which is the first change to the guest (`guest_state.py:225-235`). `bool` is rejected because the check uses `type(...) is not int`.

5. **No new regressions are introduced by the patch.**

## Areas checked and found correct
- **Own-versus-peer error routing:** an error tagged with your own session calls `relayFailed`; an error from a peer calls `transportFailed` and only affects the receive side (`FileTransferManager.cpp:349-378`). The server rejects incoming messages that already carry `relay_error`, `relay_accepted` or a retry value (`murmur/Messages.cpp:2669`), so peers can't forge these. The new manager test matches this.
- **Reset prompt scoping:** `resetPeerTrust` cancels only prompts for `shown.session`, then calls `abortPeer` for that session (`FileTransferManager.cpp:627-636`). Another peer's prompt token survives.
- **Portability:** the `qBound<qint64>` casts are harmless. `MaximumFrameBytes` at +128 now matches `packetsize = 128 + data` (`Messages.cpp:2695`).

## Should fix / release coverage limits
- **S1: the actual regression has no test.** `synchronousRelayAdmissionCompletes` covers one job, synchronous acknowledgements and no rate limit. No test runs two concurrent jobs with delayed acknowledgements and checks that the smaller one makes progress. Add one, for example by queuing `relayResponse` 5 ms later with two files of different sizes.
- **S2: `config['port']` is not validated during restore.** It is used before any change in the firewall rule match (`guest_state.py:232`), and again after the changes in `wait_ready` (`:264`). A missing or string value only fails after installation has started. The archive is pinned by digest, so the risk is low, but it should get the same check as `ssh_port`.
- **S3: a misleading error can fail a send that the server accepted.** If incoming data overflows, `ServerHandler.cpp:767-783` turns your own acceptance message into a `relay_error` tagged with your own session. That fails the outgoing transfer with "Incoming file queue is full", even though the server delivered the chunk. Failing closed is defensible, but the message is wrong.
- **S4: one transient delay.** When a refusal arrives inside the callback, the job stays at the front for that pass without rotating (`:1144-1147`). It costs up to 50 ms and does not starve anything.
- **S5: the new server check at `Messages.cpp:2699` can never trigger.** Data is capped at 1 MiB + 16 (`:2670`), so `packetsize` can't exceed `maximumBurst(rate)`. It is harmless.
- **Not verifiable from this snapshot:**
  - `MainWindow.ui` is not in the snapshot, so I can't confirm `qtbSendFile` exists when building without `USE_FILE_SHARING` (the new `#else` at `MainWindow.cpp:274-275`). That build configuration isn't covered.
  - There is no record of re-running the 96 MiB / 3 MiB transfer on a real server.
  - The retry stagger is fixed per transfer, based on the first byte of the transfer ID (`:1305-1306`). Under heavy server refusals, one job could consistently retry later than the other. This is untested.
  - `test_slow_receiver.py` exercises the server relay only, not the file engine; its docstring says so.