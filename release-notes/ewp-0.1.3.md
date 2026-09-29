# Ersa Wearable Platform ewp-0.1.3

This release adds the first on-watch A/B system updater, OTA slot diagnostics,
and live ESP-IDF power-residency reporting through `ewctl`.

## Highlights

- Added dual application slots with bootloader rollback support and a one-time
  migration script for existing watches. The migration preserves NVS settings.
- Added a compact `system update` app to check for releases, install an update,
  view the running and alternate slots, and boot a valid alternate image.
- Added a manifest-based OTA flow hosted at `https://pkgs-wearables.ersa.dev/`.
  It verifies HTTPS, image metadata, size, and SHA-256 before selecting the
  inactive slot. New firmware confirms stable startup before rollback is
  disabled.
- Updated the app drawer to show four roomier entries without page counters.
- Added `ewctl ota` slot inspection and `ewctl power get-dvfs-state` for ESP-IDF
  power-mode residency.
- Fixed Arch package dependencies for `ewctl` and updated release packaging to
  publish the firmware image and OTA manifest with each `ewp-*` release.

## Upgrade notes

Existing devices must run the included one-time OTA partition migration before
using the updater. Follow `FLASHING.md` and the bundled `migrate_ota.sh` exactly;
after migration, use the on-watch updater for routine firmware updates.

## Firmware assets

- `firmware.bin` — app image used by the OTA updater.
- `ewp-factory.bin` — factory image for new or fully reset devices.
- `migrate_ota.sh` — one-time migration from the previous partition layout.
- `SHA256SUMS` — checksums for the firmware images.
- `FLASHING.md` — image and migration instructions.
