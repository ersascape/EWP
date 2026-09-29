# Changelog

## ewp-0.1.1

- Fixed ANCS attribute requests for notifications that advertise a negative
  action, restoring message and caller details from iOS.
- Made ANCS recovery skip a UID that returns no attribute response so one stale
  notification cannot block later notifications.
- Kept the watch eligible for light sleep during passive BLE notification
  bursts; buttons reset the 30-second inactivity timer.
- Improved Apple BLE discovery/recovery, call handling and recent-call controls;
  added BLE Current Time Service preference with network time fallback.
- Added low-battery safeguards and last-session uptime reporting.
- Updated the firmware build and release notes workflow for tagged binary
  releases.

See [release notes](release-notes/ewp-0.1.1.md) for the user-facing summary.
