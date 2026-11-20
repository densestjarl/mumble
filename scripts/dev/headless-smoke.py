#!/usr/bin/env python3
# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file at the root of the Mumble source tree.

"""Connect the real client to an isolated loopback server and save evidence."""

import argparse
import base64
import contextlib
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import sqlite3
import subprocess
import sys
import time


def run(*command, **kwargs):
    return subprocess.run(command, check=True, timeout=30, **kwargs)


def wait_for(predicate, processes, description):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if any(process.poll() is not None for process in processes):
            raise RuntimeError(f"Process exited while waiting for {description}; inspect logs")
        if predicate():
            return
        time.sleep(0.1)
    raise RuntimeError(f"Timed out waiting for {description}; inspect logs")


@contextlib.contextmanager
def process(command, log, env):
    with log.open("wb") as output:
        child = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, env=env)
        try:
            yield child
        finally:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


def smoke(build, output):
    # Require a new directory: never overwrite another run's identities or logs.
    output.mkdir(mode=0o700, parents=True)
    temporary = output / "tmp"
    temporary.mkdir(mode=0o700)
    env = dict(os.environ, HOME=str(output), TMPDIR=str(temporary),
               QT_QPA_PLATFORM="xcb", LC_ALL="C.UTF-8")
    for name in ("CONFIG", "DATA", "CACHE", "RUNTIME"):
        directory = output / name.lower()
        directory.mkdir(mode=0o700)
        env[f"XDG_{name}_HOME" if name != "RUNTIME" else "XDG_RUNTIME_DIR"] = str(directory)
    # Avoid registering services on an existing desktop session bus.
    env["DBUS_SESSION_BUS_ADDRESS"] = "unix:path=" + str(output / "no-session-bus")
    with (output / "openssl.log").open("wb") as log:
        run("openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "30",
            "-subj", "/CN=AgentSmoke", "-keyout", str(output / "key.pem"),
            "-out", str(output / "cert.pem"), stdout=log, stderr=log)
        run("openssl", "pkcs12", "-export", "-inkey", str(output / "key.pem"),
            "-in", str(output / "cert.pem"), "-passout", "pass:",
            "-out", str(output / "identity.p12"), stdout=log, stderr=log)
    der = run("openssl", "x509", "-in", str(output / "cert.pem"), "-outform", "DER",
              stdout=subprocess.PIPE).stdout
    # Choose an available TCP port; the server also binds UDP to this port.
    # A competing bind in the small release/start interval fails via the logs.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    (output / "server.ini").write_text(
        f"host=127.0.0.1\nport={port}\ndatabase={output}/server.sqlite\n"
        f"sslCert={output}/cert.pem\nsslKey={output}/key.pem\n"
        "bonjour=false\nice=\nlogfile=\nwelcometext=Agent smoke connected\n")
    database = output / "client.sqlite"
    with sqlite3.connect(database) as connection:
        # Trust only this run's certificate in this run's client database.
        connection.execute("CREATE TABLE cert (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "hostname TEXT, port INTEGER, digest TEXT)")
        connection.execute("INSERT INTO cert(hostname,port,digest) VALUES (?,?,?)",
                           ("127.0.0.1", port, hashlib.sha1(der).hexdigest()))
    # Current settings are JSON; legacy INI names are not valid for --config.
    (output / "client.json").write_text(json.dumps({
        "settings_version": 1,
        "certificate": base64.b64encode((output / "identity.p12").read_bytes()).decode("ascii"),
        "misc": {"database_location": str(database), "audio_wizard_has_been_shown": True,
                 "viewed_server_ping_consent_message": True},
        "ui": {"send_usage_statistics": False},
        "update": {"check_for_updates": False, "check_for_plugin_updates": False},
        "audio": {"mute": True, "deaf": True},
    }))
    with process([str(build / "mumble-server"), "--foreground", "--ini", str(output / "server.ini")],
                 output / "server.log", env) as server:
        def listening():
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                    return True
            except OSError:
                return False
        wait_for(listening, [server], "server listener")
        with process([str(build / "mumble"), "-m", "-c", str(output / "client.json"),
                      "--default-certificate-dir", str(output), "--window-title-ext", "AgentSmoke",
                      f"mumble://AgentSmoke@127.0.0.1:{port}/"], output / "client.log", env) as client:
            wait_for(lambda: "Authenticated" in (output / "server.log").read_text(errors="replace"),
                     [server, client], "client authentication")
            time.sleep(1)  # Allow the connected channel tree to paint.
            run("ffmpeg", "-nostdin", "-loglevel", "error", "-f", "x11grab",
                "-video_size", "1280x800", "-i", env["DISPLAY"], "-frames:v", "1",
                "-threads", "1", str(output / "connected.png"), env=env)
            if client.poll() is not None or server.poll() is not None:
                raise RuntimeError("Client or server exited before screenshot completed")
    print(f"PASS: client authenticated; logs and connected.png in {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, help="Build directory containing mumble and mumble-server")
    parser.add_argument("output", type=Path, help="New directory for logs, temporary state and screenshot")
    parser.add_argument("--inside-xvfb", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    for tool in ("openssl", "ffmpeg", "xvfb-run", "Xvfb", "xauth"):
        if not shutil.which(tool):
            parser.error(f"Missing {tool}; see docs/dev/build-instructions/agent_linux.md")
    build, output = args.build.resolve(), args.output.resolve()
    for binary in ("mumble", "mumble-server"):
        if not os.access(build / binary, os.X_OK):
            parser.error(f"Build {build / binary} first")
    if not args.inside_xvfb:
        return subprocess.call(["xvfb-run", "-a", "-s", "-screen 0 1280x800x24 -nolisten tcp",
                                sys.executable, str(Path(__file__).resolve()), str(build), str(output),
                                "--inside-xvfb"])
    try:
        smoke(build, output)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}; evidence: {output}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
