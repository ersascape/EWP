# Ersa Wearable Platform ewp-0.1.5

This release improves OTA memory handling and avoids restarting BLE between an
update check and installation. It also reports current boot uptime in status.

## Highlights

- When an update is available, Bluetooth stays suspended while the updater
  waits for the Install button. Pressing B1 to leave the updater resumes BLE.
- OTA pauses companion providers through a generic `ICompanionSource`
  maintenance contract; the updater does not depend on Apple-specific APIs.
- OTA accepts a smaller contiguous free block when using the flash-resident
  certificate bundle, while still requiring sufficient total heap before Wi-Fi
  and TLS work begins.
- The updater and `ewctl ota` validate image segment extents before asking
  ESP-IDF to verify the alternate slot, so erased or partial images are simply
  reported as not bootable.
- Added a current-boot uptime row to the watch's Status screen. `ewctl status`
  renders uptime as a compact duration; JSON keeps `uptime_seconds` unchanged.

## Upgrade notes

Install from **System Update**. If the watch reports an update, BLE remains
paused until you install it or leave the updater with B1. A successful install
reboots into the new slot and restores normal Bluetooth startup.
