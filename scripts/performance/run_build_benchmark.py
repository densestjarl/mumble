#!/usr/bin/env python3
"""Measure a full incremental build after touching one source.

Start from a fully built Ninja directory. The source contents are never changed.
Keep compiler, configuration, job count, machine and source identical for both
revisions. This intentionally measures recompilation, archiving and relinking.
"""

import argparse
import json
import os
from pathlib import Path
import resource
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    original = args.source.stat()
    results = []
    try:
        for number in range(args.repeat):
            args.source.touch()
            before = resource.getrusage(resource.RUSAGE_CHILDREN)
            started = time.perf_counter()
            result = subprocess.run(["cmake", "--build", str(args.build), "-j", str(args.jobs)],
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            elapsed = time.perf_counter() - started
            after = resource.getrusage(resource.RUSAGE_CHILDREN)
            (args.output / f"{number}.log").write_text(result.stdout)
            if result.returncode:
                raise RuntimeError(f"Build failed: {args.output / f'{number}.log'}")
            results.append({"elapsed_seconds": elapsed,
                            "cpu_seconds": after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime,
                            "cxx_compilations": result.stdout.count("Building CXX object"),
                            "ui_generations": result.stdout.count("Generating ui_"),
                            "source_compilations": sum("Building CXX object" in line and args.source.name in line
                                                       for line in result.stdout.splitlines())})
            print(json.dumps(results[-1]), flush=True)
    finally:
        os.utime(args.source, ns=(original.st_atime_ns, original.st_mtime_ns))
    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
