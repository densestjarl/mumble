# PR40 review: targeted file relay, pacing/budgets, receipts, trust gating, hosting readiness

**Scope:** I read `change.diff` and the snapshot (base `6988681d1`) only. Nothing was built or run, so I make no claim that any test passes or that the change is ready to release. Line numbers below refer to the snapshot files.

---

## Must

### M1. The client doesn't compile when `USE_FILE_SHARING` is off
- **Where:** `src/mumble/Messages.cpp:1346-1359`. `msgFileTransferControl` and `msgFileData` now call `Global::get().fileTransferManager->handleControlMessage(...)` and `->handleDataMessage(...)` with no `#ifdef USE_FILE_SHARING` around them.
- **Why it breaks:** `FileTransferManager.h` is only included under that macro (`Messages.cpp:42-44`). `Global.h:62` only forward-declares the type. Calling a member function on an incomplete type is a compile error. The other file-sharing code in this file is guarded (`:134`, `:209`, `:870`, `:936`), so builds without the feature are clearly meant to work.
- **Reproduce:** configure the client with file sharing disabled and compile `Messages.cpp`.
- **Fix:** wrap both bodies in `#ifdef USE_FILE_SHARING`. Also drop the stale "Defensive stub … interception path is broken" comments, since this is now the real path.

### M2. Any peer in the channel can open an unlimited stack of nested trust dialogs
- **Before this patch:** a second M1 from a peer whose first contact was still pending was ignored.
- **Now:**
  - A replacement M1 with a different payload discards the pending handshake and creates a new request token (`FileTransferEngine.cpp:495-511`).
  - The manager erases the older request and emits a new `firstContact` (`FileTransferManager.cpp:162-187`).
  - `MainWindow::onFileFirstContact` runs synchronously and calls `SafetyNumberDialog::exec()` (`MainWindow.cpp:4946-4947`). `exec()` runs a nested event loop, so the next M1 opens another modal dialog on top of the first.
- **Reproduce:** from a capable client in the same channel, send M1 frames with a fresh ephemeral key every ~100 ms. Each one opens another dialog and adds stack depth. Any older dialog the user answers is rejected and logs "peer or connection changed".
- **Fix:**
  - Allow at most one open prompt per peer. A replacement should update that prompt's token and key in place, or be refused while the dialog is open.
  - Rate-limit replacements per peer.
  - Close dialogs that have gone stale.
  - Add a test that delivers N replacement M1s and asserts at most one prompt.

### M3. Multi-recipient sends report a false terminal state
- **Mechanism:**
  - Recipients that never finish the handshake are marked `failed` and the transfer continues to the rest (`FileTransferEngine.cpp:913-920`).
  - Both success checks require every peer to be `established && !failed && received` (`:196-200` in the receipt path, `:1191-1206` at completion).
  - Any server refusal of a chunk is fatal to the whole job: the server refuses the entire chunk if any one target has left or is backed up (`murmur/Messages.cpp:2659-2670`), and the client calls `relayFailed` → `finishSend(false)` (`FileTransferEngine.cpp:1210-1214`).
- **Reproduce:** select A and B, with B offline or leaving mid-transfer. A gets the file and sends a receipt. After 60 s the sender shows "Recipient did not confirm file delivery" (or the relay error). If the user retries, A gets a duplicate.
- **Fix:**
  - Track a terminal state per recipient (delivered / failed with reason). Finish when every live recipient is either delivered or failed.
  - The server should either deliver to the remaining valid targets and refuse per target, or the client should remove the departed target and resend the same chunk index.

### M4. The server-wide aggregate budget makes concurrent senders fail outright
- **Client side:** each client paces against 90% of the advertised aggregate divided by its own largest recipient count (`FileTransferManager.cpp:650`, `FileTransferEngine.cpp:1043-1049`).
- **Server side:** `m_bwrFileAggregate` is a single meter shared by every user (`murmur/Messages.cpp:2691-2695`). When it's exceeded the server sends a refusal, and the client treats that as fatal.
- **Result:** two users sending at the same time each use about 90% of the aggregate, so together they reach about 180%. Refusal is guaranteed and both transfers fail with "try again later", even though nothing is ever retried.
- **Reproduce:** set `filebandwidthaggregate` low enough to bind and start two senders on different clients at once.
- **Fix:** make budget refusals retryable:
  - The server marks "budget exceeded" as retryable (add a field or code; the current reason is a free-text string).
  - The client resends the same chunk index after a backoff without advancing. The receiver never saw the chunk, so it isn't a duplicate.
  - Alternatively, the server applies backpressure instead of refusing.

  The same applies to the per-user upload budget, since the client's pacing is only approximate.

---

## Should

### S1. Stale-connection tagging doesn't cover reconnects, and queued outbound data leaks into the next connection
- `connectionID` is assigned once, in the `ServerHandler` constructor (`ServerHandler.cpp:128-133`).
- Auto-reconnect and certificate-accept retry reuse the same object: `on_Reconnect_timeout` calls `sh->start()` (`MainWindow.cpp:1739-1749`, `:3879`).
- So the new check in `MainWindow.cpp:4039-4041` only separates different `ServerHandler` objects, not separate connections made by the same one.
- **Outbound:** `queueFileData` lambdas (`ServerHandler.cpp:763-786`) and `Transport::queueControl` are posted to the `ServerHandler` thread. If they're still queued when `run()` exits, they are delivered once the next `run()` starts its event loop. They are then sent on the new connection to session IDs the server may have reassigned. Their charges also stay counted in `m_fileQueuedBytes` until then.
- **Fix:** assign a new connection ID on each pass of `run()`. Capture it in the queued lambdas and drop the lambda if `cConnection` or the ID has changed.

### S2. Inbound file data now runs on the GUI thread with no bound
- `ServerHandler::message` no longer intercepts file messages (`ServerHandler.cpp:744-749`). Every chunk of up to 1 MiB is posted to the GUI thread, parsed there, copied, and queued to the engine. Neither queue has a limit.
- In debug builds, `PROCESS_MUMBLE_TCP_MESSAGE` calls `PrintDebugString()` on every message (`MainWindow.cpp:4052-4060`), so each 1 MiB chunk is dumped to stdout.
- **Fix:** route FileData directly to the engine after a connection-ID check, with a bounded inbound queue (drop or abort the transfer on overflow). At minimum, skip debug printing for file messages.

### S3. Peers can trigger a full trust-store refresh on the GUI thread with every control message
- `handleControlMessage` calls `refreshPinCache()` every time (`FileTransferManager.cpp:255`). That runs one SQLite lookup per capable user in the channel, plus the request-invalidation loop.
- **Fix:** refresh only when an M1 header arrives, or debounce. Users who change or leave already trigger refreshes (`Messages.cpp:870`, `:936`).

### S4. `configureRelay` treats missing fields as "unsupported / unlimited"
- `FileTransferManager.cpp:671-677` reads proto2 optional fields without checking `has_*()`.
- If a server ever sends a `ServerConfig` that leaves these fields out, relay support turns off and the rate and size limits become 0, which means unlimited pacing.
- **Fix:** update only the fields that are present. Reset them on disconnect, which already happens.

### S5. Receipts aren't negotiated
- Older receivers reject header type 8 (the old check was `t > TypeAbort`), so they never send a receipt. Every send to them ends in "did not confirm" after 60 s, even though the file arrived.
- In password mode, the receiver can't finish until the user types the password. The sender's fixed 60 s timer (`FileTransferEngine.cpp:1205`) can expire just before the receiver finishes draining its spool.
- Older clients on the new server send FileData with no targets. The server refuses it, and those senders still report success.
- **Fix:** advertise receipt support in the handshake or capability. Extend the sender's wait while the receiver shows progress, for example with a "waiting for password" or progress control.

### S6. Backpressure retries have no time limit and redo work
- When `queueFileData` refuses a chunk, `sendNextChunk` returns without advancing (`FileTransferEngine.cpp:1146-1151`). Every 50 ms after that it re-reads and re-encrypts up to 1 MiB (`:1098-1101`, `:1113`).
- The sender has no stall timeout. If the socket never drains, the sender stays in Transferring indefinitely, while the receiver's 60 s idle timer kills its side silently.
- **Fix:** cache the sealed chunk until it is admitted, and add a sender stall deadline that fails the send and sends Abort.

### S7. A request token of 0 still resolves any pending handshake
- At `FileTransferEngine.cpp:1496`, the token check only applies when `requestToken != 0`. The default argument in the header is 0.
- **Fix:** require the exact token, as all production callers already pass one.

### S8. The SSH port is never checked against the real `sshd`
- `historical_recovery.py:159-207` and `deploy_guest.py:301-305` apply `policy drop` with whatever `--ssh-port` was given (default 22).
- If sshd actually listens on 2222 and the flag is forgotten, new SSH sessions are blocked. Rerunning with the right port then fails "does not belong", because the marker is tied to `ssh_port` (`:170-173`). The operator can neither fix it by resuming nor get back in over SSH.
- **Fix:** before touching anything, check that sshd is listening on the given port (`sshd -T` or `ss -ltn`). Also allow a resume that only changes the SSH port to rewrite the firewall.

### S9. `wait_ready` gives a misleading error on a wrong certificate
- `ssl.SSLCertVerificationError` is an `OSError`, so a wrong or expired pinned certificate is retried until the timeout (`deploy_guest.py:169`).
- The final message then says "service may be restarting".
- **Fix:** catch certificate-verification errors separately and fail fast with an identity error.

### S10. Conditional pin removal isn't reachable from production code
- No code under `src/mumble` calls `removePin`. Only tests do.
- The two-argument overload looks up the current fingerprint and deletes that (`PeerTrustStore.cpp:93-96`), so it isn't conditional on what the user was shown.
- **Fix:** wire up the re-verify flow so it passes the fingerprint that was shown, and remove the convenience overload.

### S11. The slow-receiver hosting probe is out of date
- `scripts/hosting/test_slow_receiver.py:200-207` sends FileData with no `target_session`. The server now refuses all of it, so `received == list(range(count))` fails.
- Its `--expect-disconnect` model (a stalled receiver gets disconnected) also no longer matches the new 4 MiB refusal behaviour.
- **Fix:** add targets, and assert the new refusal and isolation behaviour.

### S12. Relay errors aren't tied to the local user (minor hardening)
- `handleDataMessage` honours `relay_error` without checking that `actor` is our own session (`FileTransferManager.cpp:277-283`).
- The server does strip `relay_error` from clients (`murmur/Messages.cpp:2703`), so this is defense in depth only.

---

## Checked and looks correct

These paths look right on reading only; I didn't test them.
- **Outbound admission bound:** `ServerHandler` is moved to its own thread (`MainWindow.cpp:1342`), so the `queueFileData` lambda writes directly to the socket. The charge is released after `sendMessage`, and `outputProgress` updates the socket counter.
- **Server targeting:** the server rejects empty or oversized target lists, client-set `relay_error`, sending to yourself, users in other channels, and non-capable users. Broadcast is gone.
- **Receive-side tokens:** request tokens are carried end to end. `resolveFirstContact` restores the latest pending prompt for the peer correctly.
- **Trust storage errors:** a storage error is never reported as "changed", and the send dialog disables those recipients.

---

## Gaps in test coverage

**`concurrentTargetedLargeDelivery`** (real murmur, real crypto):
- Drives `FileTransferEngine` directly, so `FileTransferManager`, `ServerHandler::queueFileData`, `Connection::outputProgress`, MainWindow connection filtering and `configureRelay` are never exercised.
- Never reads `ServerConfig`. The test server's ini sets no `filebandwidth`, `filebandwidthaggregate` or `maxfilesize`, so neither server budget is tested and no refusal path is hit.
- `authorizeSendPeers` is left at its default (false), so the sender-side first-use prompt is never exercised against the real server.
- The socket write always returns true, so backpressure is never tested.
- Only one recipient per job, so M3 isn't covered.

**`productionSenderAuthorization`:** despite the name, it replaces the transport with in-process engines (`m_transportSetup`), with no server and no `ServerHandler`. "storage-error" is simulated by closing the database.

**Not tested at all:**
- Replacement M1s and the prompt flood (M2).
- A send prompt and a receive prompt open for the same peer at once.
- Reconnecting on the same `ServerHandler` (S1).
- Server refusal paths: recipient left, 4 MiB backlog, per-user budget, aggregate budget.
- Two concurrent senders against the aggregate budget (M4).
- Receipt timeout or old receivers.
- Password-mode receipt timing.
- Building without `USE_FILE_SHARING` (M1).

**Hosting tests:**
- The readiness tests mock `systemctl`, the socket and TLS. Nothing runs `wait_ready` against a real murmurd, a crash-looping unit, or an old historical certificate.
- The SSH tests only check the marker and the rendered firewall text. Nothing checks the port against sshd.

**Not in the snapshot:** `Server.cpp`/`Meta.cpp`. So I couldn't check the default file-bandwidth and size limits, whether any other code path sends `ServerConfig` (relevant to S4), or whether there is a server-side pending-bytes disconnect that interacts with the 4 MiB refusal threshold.