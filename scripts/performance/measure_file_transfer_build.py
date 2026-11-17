#!/usr/bin/env python3
# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.

"""Measure clean or source-edit builds of the standalone file-transfer tests.

Use the same idle machine, Ninja build, compiler flags and job count on both
revisions. Disable compiler caches when comparing compilation work. A target
clean removes the five executables' dependency closure (including shared/server
objects), not the whole client. Source contents and timestamps are preserved.
"""

import argparse
import json
import os
from pathlib import Path
import resource
import subprocess
import time

TARGETS = ["TestPQCrypto", "TestFTIdentity", "TestFileTransferSession",
           "TestFileTransferEngine", "TestFileTransferE2E"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()
    if args.repeat < 1 or args.jobs < 1 or (not args.clean and args.source is None):
        parser.error("use positive jobs/repeat and either --clean or --source")
    args.output.mkdir(parents=True, exist_ok=False)
    original = args.source.stat() if args.source else None
    results = []
    try:
        for number in range(args.repeat):
            if args.clean:
                subprocess.run(["ninja", "-C", str(args.build), "-t", "clean", *TARGETS],
                               check=True, stdout=subprocess.DEVNULL)
            if args.source:
                args.source.touch()
            before = resource.getrusage(resource.RUSAGE_CHILDREN)
            started = time.perf_counter()
            result = subprocess.run(["cmake", "--build", str(args.build), "-j", str(args.jobs),
                                     "--target", *TARGETS], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            elapsed = time.perf_counter() - started
            after = resource.getrusage(resource.RUSAGE_CHILDREN)
            (args.output / f"{number}.log").write_text(result.stdout)
            if result.returncode:
                raise RuntimeError(f"Build failed: {args.output / f'{number}.log'}")
            row = {
                "elapsed_seconds": elapsed,
                "cpu_seconds": after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime,
                "cxx_compilations": result.stdout.count("Building CXX object"),
                "backend_compilations": sum("Building CXX object" in line and "PQFileTransfer/" in line
                                            for line in result.stdout.splitlines()),
            }
            results.append(row)
            print(json.dumps(row), flush=True)
    finally:
        if original:
            os.utime(args.source, ns=(original.st_atime_ns, original.st_mtime_ns))
    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
