I'm finishing the review: I've checked the diff, restore code and tests, protocol casts, both E2E delivery tests and the runtime artifacts.

# Final review: 520c2e6 (diff from 185c091)

**Verdict: no remaining Must blockers in delivery, trust or SSH restore.**

The diff is small. It adds the server-port check and its two test scenarios in `guest_state.py` and `test_guest_state.py`, and changes two casts in `src/murmur/Messages.cpp` from `quint64` to `uint64_t`. Nothing in the engine, relay, trust store or E2E code changed after 185c091.

## Source reasoning

**SSH restore** (`scripts/hosting/guest_state.py:225-238`)
- The archived server port now gets the same check as the SSH port: it must be an `int` (so `bool` and `'64738'` are rejected) and between 1 and 65535. This runs before `verify_ssh_port`, the firewall rule match and `fresh_or_resumable`, so a bad port stops the restore before anything on the guest changes.
- The firewall rule and `wait_ready` (line 267) now use a value that has already been checked, so the old `KeyError` path is gone.
- The new tests do catch the bug (`test_guest_state.py:166-213`). Before this fix, `missing-server-port` would have raised `KeyError`, which `assertRaises((ValueError, RuntimeError))` doesn't accept. Both new scenarios also check that nothing was changed, nothing was written to disk, and `verify_ssh_port` was never called.

**Protocol rate casts** (`Messages.cpp:612-614`)
- The values are `int`, the casts only run when the value is above 0, and `qMax(1, x/8)` keeps the result positive. The proto fields are `uint64` (`Mumble.proto:621-622`), so this behaves the same as `quint64` did.

**Delivery coverage**
- I read `concurrentTargetedLargeDelivery` (`TestFileTransferE2E.cpp:604-687`). With 128 KiB chunks, the exact counts of 768 and 24 match 96 MiB and 3 MiB, so the test would catch any duplicated or extra chunk.
- The sender is held to 900 kB/s, under the test server's 1 MB/s aggregate limit (`filebandwidthaggregate=8000000`). That makes the run about 115 s against the unchanged 180 s timeout.
- It also checks:
  - the non-recipient gets zero data and zero control messages;
  - the sender reaches `Saved` for both transfers;
  - both received files match the source hashes.
- `competingSendersRetryAggregateBudget` (`TestFileTransferE2E.cpp:689-759`) uses two 48 MiB senders at 900 kB/s each, which exceeds the 1 MB/s limit and forces retries. It requires `relayRetries > 0`, exactly 768 chunks at the recipient, and two matching hashes.
- My earlier finding that large-file delivery under the shared limit was untested was wrong. It's covered.

## Runtime evidence (separate from the source reading)
- **`mac-185c091-ctest.txt`:** 41 of 41 tests passed; OverlayTest is disabled. `TestFileTransferE2E` passed in 230 s, which fits with both long tests running. This run is from 185c091. It still applies to 520c2e6 for everything except the cast change and the Python files.
- **`slow-receiver-185c091.json`:** 40 MiB and 640 chunks sent with the slow receiver paused, and the slow receiver was not disconnected. The other recipients stayed connected and saw at most 16 ms ping, with 16 relay retries and about 7 MB RSS growth. The relay code hasn't changed since.
- **`hosting-final-python.txt`:** 70 tests OK. The file doesn't record a revision, so I'm relying on its name to tie it to the final snapshot.
- **Feature-OFF client build:** you reported it passed on Mac with `MainWindow.ui` included. I have no log for it here.

## Should (not blockers)
1. **Restore is looser than deploy.** Deploy requires the server port to be 1024–65535 and different from the SSH port (`deploy_guest.py:321`). Restore only requires 1–65535 and doesn't compare the two. Since archives are pinned by digest and created by deploy, this is about consistency, not something an attacker can use. Copying deploy's check would close it.
2. **The firewall check only matches the TCP rule.** The archived `udp dport {port} accept` line is never compared. `nft --check` runs only after the files are copied into place (line 264).
3. **No 520c2e6 build log for the cast change.** The cast is trivial, but I have no artifact showing the server compiles at the final revision.

## Coverage gaps (not code Musts)
- No release hardware or signing coverage. As you asked, I'm not counting this as a code Must.