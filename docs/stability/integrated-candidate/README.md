# Integrated stability candidate — PR #40

PR #40 consolidates native client/server stability fixes and disposable deployment/recovery rehearsals. It remains an integration candidate; physical-device, shipping and paid-hosting qualification are separate gates.

## Evidence retention

Historical raw logs, test XML, source/binary hash inventories, review snapshots and detailed run records are retained in the [qualification evidence release](https://github.com/gavmac11/mumble/releases/tag/pr40-evidence-2026-10-11). The archive contains 1,509 files frozen at `8daa4c065b9ff08540b3d69b6676215141118302`, including all 1,313 PR-added files removed from these evidence directories. The 196 records already present on master are preserved in the working tree. No private lab files were added to the archive.

Archive SHA256: `4a03155edafc422bdb61aaea4d2789aac9fae39f6f34e6a45219bd6be7f3aeec`. Every archived file was checked against the tracked source before removal; the uploaded archive was downloaded and its checksum verified. [Small archive manifest](evidence-archive.json). Extraction retains the original repository paths, including failed and superseded runs.

Future raw outputs belong in workflow artifacts; evidence underpinning a release should be copied to durable release assets before artifact expiry. Keep source commit, environment, outcome, binary checksums and links in a concise summary. Do not commit exhaustive source-file inventories or raw logs. Git commits and pinned submodules identify repository source; separately exported source inputs can have a single archive checksum.

## Previously qualified baseline

At native source `80ae9813aa12cf92e1f74f36686aef11c7301d79`, complete Release builds and all 41 enabled suites passed on available Mac, Windows and Debian Linux. Windows used strict `/W4 /WX`; Debian used warnings-as-errors with its existing restrict-warning exception. Mac used shared Homebrew dependencies with warnings-as-errors disabled and requires the available macOS 26 environment. The disabled OverlayTest was unchanged. Later source `520c2e6` adds Python archived-port validation only; 70 local hosting checks passed with both native bridges.

Real-server tests covered a 96 MiB and 3 MiB concurrent send, competing 48 MiB senders, exact hashes, authenticated delivery receipts and zero unselected file/control delivery. A stalled reader resumed after eight seconds without losing either selected connection. Historical encrypted restore/resume/reboot retained database/TLS hashes on SSH 2222; wrong-port and live-listener crash-loop cases were refused. These drills do not qualify restore of a final shipping package.

The preceding head `8daa4c0` passed [CI](https://github.com/gavmac11/mumble/actions/runs/38122154878), [preview packaging](https://github.com/gavmac11/mumble/actions/runs/38122154844), [hosting checks](https://github.com/gavmac11/mumble/actions/runs/38122154855) and [both security configurations](https://github.com/gavmac11/mumble/actions/runs/38122154821). The security comparison had zero new annotations; 33 existing results per configuration remain. Preview publication was configured to skip. Historical passes do not substitute for checks of the review corrections.

## Review follow-up

See [review corrections and current validation](review-corrections.md) and [component dispositions](pr-consolidation.md). The requested review dated October 11 is the current human gate; source-only Opus reviews are supporting evidence, not approvals.

## Release qualification still required

Installed signed clients and permissions on supported OS versions; actual network voice/video/screen sharing and device changes; endurance and filesystem faults; fresh final-package deployment/restore/reboot/interruption; and provider density, support cost and egress measurements. Available local Mac speaker/microphone/camera checks do not establish remote-media or supported-version coverage. Large fanout/control pacing, many-engine fairness, received-job retention, recipient-departure behavior and prompt deadlines remain preview constraints to qualify or restrict.
