# PR40 follow-up review (v3): source `e85f6c09a`, diff against `df16102`

I only read the files. I did not build, run tests, start the GUI, or check signing, media or shipping, so nothing below says anything passes. Every finding comes from reading `change.diff` and the tracked files in this snapshot.

## Verdict

**No remaining or new Must.** All the v2 Musts look fixed in the code. Two of the tests you described are not in this snapshot, though. See "Test coverage that doesn't match the description" below.

## The v2 Musts

| Item | Status | Evidence |
|---|---|---|
| Restore checks the archived SSH port and the exact firewall rule before ownership or mutation | Fixed | `guest_state.py:225-230`: `verify_ssh_port` checks that the port is configured and listening (`deploy_guest.py:46-56`), then the exact `tcp dport { ssh, port } accept` line is checked, then `fresh_or_resumable`. |
| Restore marker records and validates the SSH port | Fixed | `guest_state.py:195,205`. Old markers default to 22, which fails safe because a mismatch refuses. |
| Waits for stable pinned readiness before publishing ready | Fixed | `guest_state.py:259-260`: `wait_ready` (pinned leaf certificate, same InvocationID/NRestarts for 2 s) runs before `save(state)`. A crash-loop leaves the `restoring` marker in place, so the restore can be resumed. Backup now uses the same `wait_ready` (`:129`). |
| File-only token bucket: idle credit capped, check/commit stays atomic | Fixed | `BandwidthRecord.cpp:426-450`. Capacity is rate + `MaximumFrameBytes`. Elapsed time is clamped to `capacity/rate+1`, so even at `INT_MAX` the maths stays under about 4.3e15 and cannot overflow `qint64`. Both methods lock the mutex. In `Messages.cpp:2696-2738`, all checks happen before the commits under `m_qmFileTransferBytes`. Voice `BandwidthRecord` is unchanged. |
| Fanout larger than the aggregate burst is refused terminally | Fixed | `Messages.cpp:2704-2708` |
| Client chunk size follows its share of the aggregate | Fixed | `FileTransferEngine.cpp:801-806`; `refillSendCredit` is clamped to at least 1. |
| Identity reset aborts only that peer and cancels only that peer's prompts | Fixed | `FileTransferManager.cpp:627-636` and `FileTransferEngine.cpp:1608-1629`. Ready jobs from other peers are left alone. |
| `abortAll` emits token retirement | Fixed | `FileTransferEngine.cpp:1642` |
| E2E sets 128 KiB chunks explicitly; counts stay exact | Fixed | Each send has one recipient, so share/4 = 225000, which is at least 131072, and the chunk stays 128 KiB. That gives 96 MiB → 768 and 3 MiB → 24 in the large-delivery test, and 2×48 MiB → 768 in the competing-senders test. |
| E2E socket callback lifetime | Fixed | The destructor now does `abortAll`, clears the transports, then `delete m_socket`, which disconnects the `readyRead` lambda that captured `this`. |

## The other items you asked about

- **Per-connection ACK reserve** (`ServerHandler.cpp:765-790`). The extra 64 KiB is only granted to small server responses that carry `relay_accepted` or `retry_after`. A peer can't fake one into the reserve, because the server refuses client FileData that carries those fields (`Messages.cpp:2668-2669`). If even the reserve overflows, the synthesized error keeps your own session as the actor and ends up in `relayFailed`, so it fails explicitly. No issue.
- **Send-vs-receive error routing** (`FileTransferManager.cpp:365-379`). Errors whose actor is your own session go to `relayFailed`; everything else goes to `transportFailed(actor)`. `transportFailed` no longer falls through to send jobs, which closes the spoof where a peer reused a sender's transfer ID. The server can only originate `relay_error` as the sender's own actor.
- **Captured prompt name and direction.** The name comes from the captured `username`, after `contactRequestCurrent` has been checked, and is HTML-escaped. One display problem is listed under Should #1.

## Test coverage that doesn't match the description

1. **There is no "peer incoming-error spoof" test.** Nothing under `src/tests` calls `transportFailed`, and no test sends a `relay_error` FileData through `FileTransferManager::handleDataMessage`. The E2E harness sends every `relay_error` straight to `relayFailed` (`TestFileTransferE2E.cpp:252-255`), so the new actor routing is never exercised. **Needed test:** start a send with transfer ID X, then deliver a FileData with `relay_error` and `actor = peer` and the same ID X. Assert the send keeps going. Then repeat with `actor = own session` and assert it fails.
2. **The reset test is weaker than described.** `peerResetKeepsUnrelatedReadyFileAndRetiresPrompt` calls `abortPeer(3)` on the engine. Session 3 is not the peer that owns the Ready file, so the test does not show that the reset peer's own transfers are aborted. It also never runs `FileTransferManager::resetPeerTrust`, so the manager-side prompt cancellation is untested. `explicitResetRejectsReplacedIdentity` only checks the pin state. **Needed test:** at manager level, with prompts open for peer A and peer B, reset A. Assert `firstContactCancelled` fires only for A's token, B's request is still current, and A's sends and receives end up Aborted.

The idle-burst, integer-width, SSH/firewall-mismatch and crash-loop tests are present and match their descriptions.

## Release-level Should

1. **The direction label is probably invisible on macOS.** Direction is only in the window title (`FileTransferDialogs.cpp:81`). The dialog is opened with `open()` on a parent, which makes it window-modal, and on macOS that becomes a sheet with no title bar. Put "before sending" / "before receiving" in the body text as well.
2. **A large fanout can starve under the aggregate bucket.** Example with the defaults (8 MB/s aggregate, 2 MB/s per sender): four single-recipient senders keep the shared bucket near empty. A 64-recipient fanout needs about 2 MB of credit per chunk (64 × (31250+144)), so its 250 ms retries may never be admitted until the others finish. There is no reservation or fairness. Add a reservation or aging mechanism, or document it next to the existing 64-peer pacing limit.
3. **The frame-size constant doesn't match what the server charges.** `MaximumFrameBytes = 1 MiB + 50`, but the server charges `packetsize = data + 128`, up to 1 MiB + 144. Two consequences:
   - The per-sender check has no terminal "exceeds burst" refusal. A 1 MiB-chunk client on a server with `filebandwidth` under about 752 bit/s would be told to retry forever. The shipped client doesn't choose 1 MiB chunks at that rate, so only third-party clients hit this.
   - PROTOCOL.md says "1 MiB + tag/framing", which doesn't match either.

   Fix: set the constant to `MaxChunk + 16 + 128` and add a terminal check on the per-sender path like the aggregate one.
4. **Some valid configs always refuse terminally.** Because chunks can't go below `MinChunkSize` (16 KiB), a 64-recipient fanout is always refused when the aggregate rate is under about 9.2 KB/s, i.e. 64 × 16528 − (1 MiB + 50). The same happens if an admin lowers `filebandwidthaggregate` at runtime (`Server.cpp:522`), because clients keep the rate they were told at login. Document a minimum, or refuse at `startSend`.
5. **Old or malformed archives crash restore with an uncaught exception.** An archive with no `config.ssh_port`, or a non-integer value, raises `KeyError`/`TypeError` at `guest_state.py:225-226`. `main()` doesn't catch those, so you get a traceback instead of the `State operation stopped` message. Nothing has been changed at that point, but validate the type and range and raise `ValueError`.
6. **A readiness failure during backup hides the real error.** If the snapshot fails and then `wait_ready` in the `finally` (`guest_state.py:129`) also raises, only the readiness error is reported. Chain it or log both.
7. **Resetting one recipient aborts the whole send.** `abortPeer` cancels every multi-recipient send that includes the reset peer, other recipients included. This is the documented all-or-none behaviour, but PROTOCOL.md says "cancels transfers… with that peer", which reads narrower. Reword it.
8. **Small cleanups:**
   - The `BandwidthRecord.h` comment says file budgets don't accumulate idle credit; they do, up to one second plus one frame.
   - `abortPeer`'s send loop dereferences `m_sendJobs.value(id)` without a null check. The receive loop has one. It's safe today but fragile.

## Limits that stay open (not counted as qualified)

These are still follow-ups and nothing in this diff closes them:

- GUI sustained-overflow behaviour
- 64-peer control-rate pacing, plus the fanout starvation in Should #2
- The 32 unsaved Ready jobs cap
- Recipient departure and all-or-none retries, including the reset abort in Should #7
- Multi-prompt deadlines

Not verified at all: test results, GUI behaviour (including the macOS sheet rendering), signing, media, the Debian guest rehearsal, and shipping.