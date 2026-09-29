## ewp-0.1.1

This update improves Apple notification delivery, call controls, and idle power
handling for the Ampere Works T1E.

### What's changed

- Fixed the ANCS request format for notifications with a negative action. iOS
  notifications can now return their title and message instead of timing out.
- Prevented one unresponsive or stale ANCS notification UID from blocking newer
  notification details.
- Added inactivity-based light sleep after 30 seconds without button input.
  BLE events and the once-per-minute RTC update wake the UI task; active calls
  and the setup portal keep it awake.
- Improved Apple BLE discovery and recovery, incoming-call controls, and recent
  call navigation and dialing.
- Prefer iPhone BLE Current Time Service sync, with network time as a fallback.
- Added low-battery handling and a boot log entry for the previous session's
  uptime.
- Published app and factory firmware images with checksums and flashing
  instructions through the tagged-release workflow.

### Firmware files

- `firmware.bin` — routine update; keeps saved Wi-Fi and watch settings.
- `ewp-factory.bin` — factory image; resets saved settings.
- `SHA256SUMS` — checksums for the release files.
- `FLASHING.md` — flashing steps and image selection guidance.

Automatic light sleep and battery runtime still need measurement on the
assembled watch. Battery percentage remains voltage-estimated because this board
has no fuel gauge/current sensor.
