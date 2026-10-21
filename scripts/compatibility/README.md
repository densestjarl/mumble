# Official Mumble compatibility checks

These scripts exercise a fork build against unmodified official reference builds.
They use disposable loopback servers, settings and SQLite databases. Never point
these scripts at a production database or reuse an interop output directory.

On Debian/Ubuntu the harness needs `protobuf-compiler`, `python3-protobuf`,
`python3-pyatspi`, `python3-dbus`, `python3-pil`, `xvfb`, `xauth`, `xdotool`, and
`openssl`, in addition to the application build dependencies.

Compare protobuf descriptors and TCP/UDP IDs with an official source checkout:

```sh
/usr/bin/python3 scripts/compatibility/check_protocol.py /path/to/official/source
```

Existing field names, numbers, types, labels, defaults and enum values must stay
unchanged. New fields must be optional/repeated, including fields in new messages.
The script also checks transport IDs for removals, renumbering and collisions.
Review version negotiation, feature gating and PROTOCOL.md separately.

Exercise a real GUI client under Xvfb, with a TLS observer verifying the server's
view of channel joins, moves and text delivery:

```sh
dbus-run-session -- xvfb-run -a -s '-screen 0 1440x1000x24' \
  /usr/bin/python3 scripts/compatibility/run_interop.py \
  --client /path/to/client --server /path/to/server \
  --fork-client --output /tmp/mumble-interop-new-run --seconds 125
```

Use `--fork-client` when the client is the fork, and `--fork-server` when the
server is the fork. For the official 1.5 server, also pass `--legacy-server` to use
its older command-line flags. Both client generations use JSON settings. The
legacy desktop RPC interface is supported by both generations and drives channel
joins. AT-SPI drives chat entry and checks user lists and extension controls.
The harness isolates each accessibility bridge's runtime socket, which permits
concurrent fixtures. It accepts only the fixture's self-signed loopback certificate.

Results, client/server logs, observer frame metadata and screenshots go into the
output directory. A passing mixed connection lasts at least the requested time
and receives another text message at the end. Fork clients must keep video/file
sending disabled against official servers; a fork-to-fork fixture checks the
positive capability path. Automatic file setup must not prompt when file sharing
is disabled or unsupported. SIGTERM after the final measurement is intentional
fixture cleanup, recorded separately from client liveness. A server shutdown
requiring SIGKILL is recorded as `server_shutdown_timeout` for investigation.

To test database/config loading in another server, copy a completed fixture with
SQLite's backup API and verify its stored SuperUser authentication and channel:

```sh
/usr/bin/python3 scripts/compatibility/check_database.py /tmp/mumble-interop-new-run \
  --server /path/to/other/server --output /tmp/mumble-db-new-run
```

Again use `--legacy-server` for the official 1.5 server. The original fixture is
never modified. Test both directions on separate copies: an upgrade succeeding
does not establish that a stable server can load a newer database schema.
