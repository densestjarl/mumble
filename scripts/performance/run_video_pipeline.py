#!/usr/bin/env python3
"""Run the production video pipeline over loopback TLS through a real server.

Linux/glibc only. Invoke under xvfb-run with a 1920x1080x24 screen. Each sample
uses separate sender/receiver processes, a fresh database, and 15 warmup frames
followed by 150 measured frames. No client credentials or user config are used.
"""

import argparse
import json
import os
from pathlib import Path
import socket
import statistics
import subprocess
import time


def process_usage(pid):
    # The executable name may contain spaces; stat fields start after ')'.
    fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    status = Path(f"/proc/{pid}/status").read_text().splitlines()
    rss = next(int(line.split()[1]) for line in status if line.startswith("VmRSS:"))
    return {"cpu_seconds": (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK"),
            "rss_kib": rss}


def stop(process):
    if process is None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def sample(args, number):
    root = args.output / str(number)
    root.mkdir(parents=True)
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    config = root / "server.ini"
    config.write_text(f"host=127.0.0.1\nport={port}\ndatabase={root / 'server.sqlite'}\n"
                      "bonjour=false\nautobanAttempts=0\n"
                      "videobandwidth=20000000\nvideobandwidthaggregate=20000000\n")
    receiver = sender = None
    with (root / "server.log").open("w") as server_log, (root / "sender.log").open("w") as sender_log, \
            (root / "receiver.log").open("w") as receiver_log:
        server = subprocess.Popen([str(args.server), "--foreground", "--ini", str(config)],
                                  stdout=server_log, stderr=server_log)
        try:
            deadline = time.monotonic() + 15
            while True:
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError(f"Server failed to start: {root / 'server.log'}")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=.1):
                        break
                except OSError:
                    time.sleep(.05)
            env = {**os.environ, "LD_PRELOAD": str(args.allocator)}
            env.pop("QT_QPA_PLATFORM", None)
            receiver = subprocess.Popen([str(args.client), "receiver", str(port), str(root / "receiver.json")],
                                        env=env, stdout=receiver_log, stderr=receiver_log)
            deadline = time.monotonic() + 10
            while not (root / "receiver.json.ready").exists():
                if receiver.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError(f"Receiver failed to authenticate: {root / 'receiver.log'}")
                time.sleep(.05)
            before = process_usage(server.pid)
            started = time.monotonic()
            sender = subprocess.Popen([str(args.client), "sender", str(port), str(root / "sender.json")],
                                      env=env, stdout=sender_log, stderr=sender_log)
            if sender.wait(timeout=35) != 0 or receiver.wait(timeout=5) != 0:
                raise RuntimeError(f"Client failed: inspect logs under {root}")
            seconds = time.monotonic() - started
            after = process_usage(server.pid)
            result = {"cpu_seconds": after["cpu_seconds"] - before["cpu_seconds"],
                      "rss_kib": after["rss_kib"], "seconds": seconds}
            (root / "server.json").write_text(json.dumps(result, indent=2) + "\n")
        finally:
            stop(sender)
            stop(receiver)
            stop(server)
    sender_data = json.loads((root / "sender.json").read_text())
    receiver_data = json.loads((root / "receiver.json").read_text())
    if sender_data["frames"] != 150 or receiver_data["frames"] != 150:
        raise RuntimeError("Dropped video frames; sample is invalid")
    if len(sender_data["frame_ms"]) != 150 or len(receiver_data["frame_ms"]) != 150:
        raise RuntimeError("Missing frame timing measurements")
    if not max(row[4] for row in sender_data["allocations"]) or not max(row[5] for row in receiver_data["allocations"]):
        raise RuntimeError("Codec timing interposition did not run")
    return sender_data, receiver_data, result


def summarize(rows):
    summary = {}
    for role, index in (("sender", 0), ("receiver", 1)):
        data = [row[index] for row in rows]
        values = {
            "frame_ms_mean": [statistics.mean(row["frame_ms"]) for row in data],
            "codec_ms_mean": [statistics.mean(a[4 if role == "sender" else 5] / 1e6
                                               for a in row["allocations"]) for row in data],
            "frame_ms_p95": [sorted(row["frame_ms"])[142] for row in data],
            "cpu_percent": [100 * row["cpu_seconds"] / row["seconds"] for row in data],
            "peak_rss_mib": [row["peak_rss_kib"] / 1024 for row in data],
            "event_loop_gap_ms_p99": [sorted(row["event_loop_gap_ms"])[int(.99 * (len(row["event_loop_gap_ms"]) - 1))] for row in data],
        }
        if role == "sender":
            values.update({
                "packetize_ms_mean": [statistics.mean(row["packetize_ms"]) for row in data],
                "packetize_allocation_calls_per_frame": [statistics.mean(a[0] for a in row["packetize_allocations"]) for row in data],
                "packetize_allocation_bytes_per_frame": [statistics.mean(a[1] for a in row["packetize_allocations"]) for row in data],
                "allocation_calls_per_frame": [statistics.mean(a[0] for a in row["allocations"]) for row in data],
                "allocation_bytes_per_frame": [statistics.mean(a[1] for a in row["allocations"]) for row in data],
                "large_allocations_per_frame": [statistics.mean(a[2] for a in row["allocations"]) for row in data],
                "large_allocation_bytes_per_frame": [statistics.mean(a[3] for a in row["allocations"]) for row in data],
                "encoded_bytes_per_frame": [row["encoded_bytes"] / row["frames"] for row in data],
                "tunnel_bytes_per_frame": [row["tunnel_bytes"] / row["frames"] for row in data],
            })
        summary[role] = {name: {"median": statistics.median(samples), "min": min(samples), "max": max(samples)}
                         for name, samples in values.items()}
        summary[role]["encoded_sha256" if role == "sender" else "decoded_sha256"] = \
            [row["encoded_sha256" if role == "sender" else "decoded_sha256"] for row in data]
        if role == "sender":
            summary[role]["input_rgb_sha256"] = [row["input_rgb_sha256"] for row in data]
    summary["server"] = {"cpu_percent": [100 * row[2]["cpu_seconds"] / row[2]["seconds"] for row in rows],
                         "rss_mib": [row[2]["rss_kib"] / 1024 for row in rows]}
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--allocator", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeat", type=int, default=5)
    args = parser.parse_args()
    for name in ("client", "server", "allocator", "output"):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for number in range(args.repeat):
        rows.append(sample(args, number))
        print(f"Completed sample {number + 1}/{args.repeat}", flush=True)
    (args.output / "summary.json").write_text(json.dumps(summarize(rows), indent=2) + "\n")


if __name__ == "__main__":
    main()
