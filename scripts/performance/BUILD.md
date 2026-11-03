# Incremental build measurements

For a repeatable incremental full build after touching one source:

```sh
python3 scripts/performance/run_build_benchmark.py --build build \
  --source src/mumble/VideoFramePacketizer.cpp \
  --output /tmp/build-baseline --repeat 3 --jobs 2
```

Start from an up-to-date build. Use identical compiler flags, job count and
compiler-cache settings for both revisions. The script preserves source contents
and restores its original timestamp. Record clean build and full CTest wall time
separately with `/usr/bin/time`; avoid building while timing runtime workloads.
