# Ersa Wearable Platform ewp-0.2.0

This release hardens OTA updates and BLE reconnection on Ampere Terra.

- OTA now downloads the inactive-slot image in verified HTTP byte ranges. If a
  connection resets, it resumes at the last written byte and retries with
  bounded backoff. The complete image must match the manifest size, embedded
  version, and SHA-256 before the boot slot changes.
- Updater logs now report Wi-Fi, manifest, range, image-validation, and
  boot-slot phases, including downloaded byte counts and retry failures.
- BLE reconnects reuse the cached GATT service map. A changed peer or GATT
  database gets a new client after disconnect, avoiding unsafe descriptor
  teardown on routine reconnects.
- Status and `ewctl status` show the compiled firmware version.

Existing devices already migrated to the A/B partition layout can install this
release from the on-watch System Update app. New devices can use the factory
image attached to this release.
