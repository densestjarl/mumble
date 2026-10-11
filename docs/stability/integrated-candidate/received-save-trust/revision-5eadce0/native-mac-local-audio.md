# Disposable Mac local audio check

The integrated-review client at source `5eadce0753b634147f97746e192ddef613019a02` used its own profile and explicit SQLite path. Its ad hoc signature passed strict verification. Binary hashes and precise observation limits are in [the accompanying evidence](native-mac-local-audio.json).

The audio wizard selected **MacBook Pro Speakers** and **MacBook Pro Microphone**. The user confirmed clear speaker audio, a moving microphone level bar, and clear local voice playback. The wizard was canceled afterward; the main window remained muted and deafened.

These observations qualify local audio on this Mac only. Network voice, device hot-swap, camera, screen sharing, supported OS coverage, and shipping-package signing remain open. The log reports an input device change listener warning. Although screen sharing is enabled in this build, checking Share video produced no visible source picker or preview; the toggle was then turned off and its unchecked state verified. The connected loopback server source was not independently reverified, and the locked file-transfer identity prevented a live GUI file-transfer check.
