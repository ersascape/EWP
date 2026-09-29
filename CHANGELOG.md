# Changelog

## ewp-0.1.2

- Split the companion-provider contract (`ICompanionSource`) from BLE transport
  (`IBluetooth`) so Apple, Android, and MPRIS integrations can be added as
  independent sources. Apple ANCS/AMS/CTS remains the implemented provider.
- Added a bounded USB Serial/JTAG control bridge and `ewctl` host CLI for system,
  battery, BLE, power, and log inspection.
- Added a temporary CPU-frequency test override: pin to 40, 80, or 160 MHz, or
  restore automatic 40–160 MHz power management with `cpu_mhz: 0`.
- Expanded battery sample reporting, diagnostic log access, architecture docs,
  and simulated notification/call/media previews.

See [release notes](release-notes/ewp-0.1.2.md) and the
[feature catalog](docs/FEATURES.md) for the full feature inventory and caveats.

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
