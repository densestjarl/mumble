# PR40 v2 review: targeted file relay, receipts, sender gating, hosting readiness

**Scope.** I reviewed `change.diff` against the base `6988681d1` snapshot by reading the code only. I ran nothing, so none of this says that any test passes or that the build is release-qualified. The older work-order docs were not used as evidence.

## Fixes that check out in the code

- **Relay protocol (v1).**
  - Chunks go only to listed recipients (field 6). Unselected users get nothing.
  - The server refuses client-set relay fields, discards unknown fields, and checks every recipient before charging or sending (`src/murmur/Messages.cpp:2659-2683`).
  - Admission is all-or-nothing: the server checks the budgets with `canAddFrame` and only charges with `addFrame` after the chunk is accepted, so a retried chunk is never charged (`src/murmur/Messages.cpp:2688-2717`).
- **Sender per-chunk ACK.**
  - The sender keeps the encrypted chunk (`pendingCiphertext`) and resends it unchanged on retry; it only moves to the next chunk on an ACK with a matching index.
  - Retry delays are capped at 100 ms–5 s plus jitter.
  - A 60 s stall timer is in place (`FileTransferEngine.cpp:1149-1215`, `1261-1292`, `1032-1043`).
- **Receipts.**
  - The receipt is sealed under that recipient's own session with that transfer's ID, and is only sent after the Merkle check passes (`FileTransferEngine.cpp:756-761`, `184-210`). The sender shows "Waiting for delivery confirmation…" until it arrives.
  - Recipients must advertise the receipt capability (field 26). Both the client's recipient list and the server's relay check it.
- **Partial-delivery state.** `finishSend` reports `PartiallyDelivered` or `Failed` and sends Abort only to recipients that never confirmed.
- **All-or-nothing handshakes.** If any selected recipient fails the handshake, no file content is sent (`FileTransferEngine.cpp:929-935`).
- **Sender gating.**
  - Every send goes through the authorization step (`authorizeSendPeers = true` in production).
  - The manager re-checks the selection context: user, name, server digest, connection generation and channel. It refuses on `StorageError` and `Changed`.
  - `pinPeer` re-checks channel and generation before pinning.
- **Worker request tokens.** Engine `resolveFirstContact` now requires a non-zero token that exactly matches the pending request (`FileTransferEngine.cpp:1617-1621`).
- **Connection fencing and bounded queues.**
  - Inbound events are tagged with their connection ID and dropped in both `customEvent` handlers if the connection has changed.
  - Outbound file data and controls carry the expected connection ID and have byte budgets.
- **Trust storage.**
  - `StorageError` is a distinct state, and peers whose trust lookup failed are marked unavailable.
  - `removePin` only deletes when the stored fingerprint matches, and checks that exactly one row was removed.
- **Hosting scripts.**
  - Deploy and the historical restore check that the SSH port is both configured in sshd and listening.
  - The historical resume marker is bound to `ssh_port`.
  - Readiness now requires the same service run (InvocationID + NRestarts), the exact archived TLS leaf certificate, and a Mumble Version greeting, held for 2 s across 1 s probes. With `autobanSuccessfulConnections=false`, accepted probe connections clear the autoban counter (`Server.cpp:1619`, `Meta.cpp:982-993`), so the probes won't get localhost banned.

## Must fix

### M1. Backup restore can lock out SSH on a custom port

- **Where:** `scripts/hosting/guest_state.py:225-254`.
- **Problem:** `restore()` writes the archived `firewall.nft` and enables it. That file allows only the SSH port saved in `state['config']['ssh_port']` (from the original deploy). Nothing calls `deployment.verify_ssh_port` on the restore target, and the resume marker (`fresh_or_resumable`, `:185-205`) doesn't record the SSH port. The `historical_recovery.py` fix was not carried over to this path.
- **Repro:**
  1. Deploy with `--ssh-port 2222`, then run `guest_state.py backup`.
  2. Restore onto a fresh Debian 12 guest where sshd listens on port 22.
  3. `mumble-hosting-firewall.service` loads a policy-drop table allowing only `{2222, 64738}`. Only the current SSH session survives (via established/related); new SSH connections and automation reconnects are dropped.
- **Fix:**
  - After `unpack_verified_archive` and before `fresh_or_resumable` (and before stopping the service or running apt), call `deployment.verify_ssh_port(state['config']['ssh_port'])`.
  - Also cross-check that the archived `firewall.nft` contains that exact port.
  - Record `ssh_port` in the `restoring` marker and require it to match on resume.
  - Add a restore test with a mismatched port. There is currently no restore success-path test at all.

### M2. Server file budgets don't limit bursts after idle time

- **Where:** `src/murmur/BandwidthRecord.cpp:49-65`, which `msgFileData` relies on at `src/murmur/Messages.cpp:2686-2717`.
- **Problem:** The meter compares the sum of the last 360 frames against the time since the oldest slot was written. A new connection's slots start when it connects, and an idle server's aggregate slots can be hours old. So the allowance grows with idle time and is admitted immediately.
- **Repro:**
  1. Run a server with `filebandwidthaggregate=8000000` (1 MB/s), left idle for 10 minutes.
  2. A non-pacing client (the client's own 90% pacing is voluntary) sends 1 MiB chunks to 10 recipients.
  3. Roughly 600 MB of fan-out (about 57 chunks × 10 recipients) is admitted back-to-back. The only limit is the per-recipient 4 MiB pending gate, which drains at line rate. The per-user `filebandwidth` meter behaves the same way, measured from connection time.
- **Why it's Must:** The meter itself is pre-existing, but this patch now advertises these rates (`ServerConfig` 10/11) and relies on them as the server budget. This undercuts the budget claim and lets file traffic starve voice.
- **Fix:** Use a token bucket for `bwrFile` and `m_bwrFileAggregate`, with burst capacity of about 1 s of rate plus one maximum frame. Keep check-then-commit under the same lock. Add a test that admits after a long idle period.

### M3. "Reset file identity" for one peer aborts every transfer and deletes unrelated verified files

- **Where:** `FileTransferManager.cpp:557-565`, which calls `abortAll()`. Engine behaviour is at `FileTransferEngine.cpp:1420-1445` and `1593-1607`.
- **Problem:** `abortTransfer` sets receive jobs to `Aborted` before `cleanupReceive`, so `keepTemp` is false. Every Ready-but-unsaved file from any peer has its verified temp copy deleted. Every in-flight send and receive with other peers is cancelled.
- **Second problem:** The engine's parked first-contact handshakes are cleared without emitting `firstContactRetired`. The manager's matching `m_contactRequests` and open dialogs stay alive. Accepting one later still calls `pinPeer`, which persists a pin from an unauthenticated M1 claim with no handshake behind it.
- **Repro:**
  1. Have a verified, unsaved file from C, plus a transfer in progress to D.
  2. Open B's context menu, choose "Reset file identity…", and confirm.
  3. C's card goes to Aborted and its temp directory is removed; the transfer to D is cancelled.
- **Fix:**
  - Add `abortPeer(session)` that only aborts send jobs including that session, receive jobs from it, and its pending handshake (emitting `firstContactRetired`).
  - Cancel and erase only that peer's contact requests.
  - Test that a third party's Ready file survives the reset.

### M4. The new real-server E2E tests assert chunk counts the engine won't produce

- **Where:** `src/tests/TestFileTransferE2E/TestFileTransferE2E.cpp:637-638` and `:703`.
- **Problem:** The tests set `sendRateBytesPerSecond = 900000`. `startSend` then caps chunk size at `max(16 KiB, rate/4) = 225000` bytes (`FileTransferEngine.cpp:788-794`), so the expected chunk counts are wrong:

  | File | Asserted | Actual |
  |---|---|---|
  | 96 MiB | 384 | 448 |
  | 3 MiB | 12 | 14 |
  | 2 × 48 MiB | 384 | 448 (224 each) |

  `receivedFileData` counts every FileData frame (`:232`).
- **Consequence:** As written, `concurrentTargetedLargeDelivery` and `competingSendersRetryAggregateBudget` would fail. They can't be counted as evidence for targeted relay, aggregate retry, or 96 MiB delivery.
- **Fix:** Derive the expected counts from the manifest (`ceil(size / effectiveChunkSize)`), or set a rate whose `rate/4` is at least 256 KiB. Then actually run them.

## Should fix

1. **The client's inbound overflow can fail our own sends.** In `ServerHandler.cpp:762-784`, every FileData frame shares the 4 MiB budget, including tiny server ACK/retry frames. When the budget is full (say the GUI is slow while we're also receiving a file), the server's ACK for our own chunk becomes a `relay_error` with `actor` set to us. `transportFailed` then fails our send, even though the server already delivered that chunk.
   - On the same path, a peer's flood is reported with that peer as `actor` and an attacker-chosen `transfer_id`. `transportFailed` (`FileTransferEngine.cpp:1300-1304`) checks `m_sendJobs` before checking the actor. A recipient who knows our transfer ID can therefore kill our multi-recipient send.
   - **Fix:** Exempt `relay_accepted`/`relay_retry_after_ms` frames from the budget. Only route `relay_error` to a send job when `actor == own session`.
2. **File traffic now goes through the GUI thread, and overflow fails instead of slowing down.** Previously `ServerHandler::message` sent file records straight to the engine. Now they're posted to the GUI thread (`MainWindow.cpp:4057`, `Messages.cpp:1349-1369`) with a hard 4 MiB limit at the network thread and 8 MiB at the manager. At a 2 MB/s server rate, a GUI stall of about 2 s aborts incoming transfers. If the receive is in `WaitingPassword` (not in `m_activeReceived`), dropped chunks are silent, and the transfer only fails later via idle or receipt timeouts.
   - **Fix:** Apply backpressure (pause socket reads, or bounded waits) instead of fail/drop, or keep the network-thread → engine path with connection-ID fencing.
3. **Silent DoS once 32 receive jobs exist.** Ready unsaved jobs stay in `m_receiveJobs`, and `startResponder` returns silently once there are 32 (`FileTransferEngine.cpp:550`). A peer who is already pinned can send 32 tiny files, after which all incoming transfers silently time out. Count only non-terminal jobs, or reply with an Abort.
4. **The server's control rate limit doesn't fit the client's 64-recipient cap.** With the documented `filecontrollimit` 20/s and burst 100 (`PROTOCOL.md:56-57`, `Messages.cpp:2614`), about 50+ recipients means M1 plus M3 bursts exceed the bucket. Records are dropped silently, and the all-or-nothing handshake fails ("Not every selected recipient could be reached"). Dropped progress records also break the strict `seq` order, so later receipts fail. Pace control frames on the client, cap recipients to fit the bucket, or have the server return a refusal.
5. **One recipient blocks or fails the whole send.** Admission is all-or-nothing per chunk:
   - A paused recipient over 4 MiB pending means every recipient gets retries.
   - A recipient leaving the channel produces a fatal refusal (`Messages.cpp:2671-2681`) and the whole send fails.

   A single stalled member fails a multi-recipient send after 60 s. Consider dropping that recipient (mark it failed, keep going) and finishing as `PartiallyDelivered`.
6. **User cancel and disconnect hide confirmed deliveries.** `abortTransfer` and `abortAll` always report `Aborted` (`FileTransferEngine.cpp:1420-1437`), even when some recipients already sent receipts. Route these through the same partial-delivery logic as `finishSend`.
7. **The sender authorization window is too short for several new recipients.** Each M4 restarts a single 60 s job timer (`FileTransferEngine.cpp:299-302`). The dialogs are shown one at a time (`MainWindow::drainFileVerifications`) and need an out-of-band safety-number comparison. With 3 or more new recipients the send usually times out. Use per-peer deadlines that pause while that peer's dialog is open.
8. **Relayed chunks reveal who else received the file.** The forwarded `msg` still contains `target_session` (`Messages.cpp:2720-2722`). Call `clear_target_session()` before relaying.
9. **Prompt wording and names.**
   - The dialog uses the live `user->qsName` rather than the name captured with the request (`MainWindow.cpp:757`).
   - The send-authorization and receive prompts look identical. Pass the captured name and the direction into `SafetyNumberDialog`.
10. **Docs are out of date.** `PROTOCOL.md:62-110` still describes FileData as a channel broadcast and lists only control types 5–7. It needs the targets, relay responses, receipts/progress (types 8/9), the receipt capability, and `ServerConfig` 9–11.
11. **Backup restart doesn't use the new readiness check.** The backup path still restarts the service with only `systemctl is-active` (see `test_guest_state.py:143-146`); it should use `wait_ready`.

## Test coverage limits

- **Hosting tests.**
  - In `test_deploy_guest.py`, `SSHPortTests` is defined after `if __name__ == '__main__': unittest.main()` (`:97-100`), so `python3 test_deploy_guest.py` never runs it.
  - There is no `guest_state.restore` success-path or SSH test.
- **Manager test.** `productionSenderAuthorization` installs the chunk transport without requiring relay ACKs, so the production ACK path is only covered by the engine unit test and the (currently inconsistent) E2E tests.
- **E2E and engine tests** never set `authorizeSendPeers`, so the production sender gating runs only in the manager fixture, not over a real server.
- **Not tested anywhere:**
  - `MainWindow`'s async dialog queue and cancellation.
  - Connection-ID fencing in `customEvent`.
  - The ServerHandler overflow path.
  - The server's fatal-refusal and recipient-departure paths.
  - The aggregate meter after idle time (M2).
  - Multi-recipient fan-out above the control bucket.
  - `transportFailed` actor spoofing.
- **`test_slow_receiver.py`:** I can't tell from the snapshot whether it has been run against this server. It now asserts nobody gets disconnected and no chunks are lost, which depends on items 2 and 5 above.
- **Hosting assumptions:** readiness assumes a self-signed leaf that works as its own trust anchor, and Python 3.11 (no strict X.509 flag), as on Debian 12. Archived historical certificates weren't examined.