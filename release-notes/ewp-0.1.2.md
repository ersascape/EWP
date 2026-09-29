# Ersa Wearable Platform ewp-0.1.2

This release makes companion integrations pluggable, adds a USB development
control bridge, and exposes a CPU-frequency override for power testing. The
[feature catalog](../docs/FEATURES.md) lists the capabilities included in the
firmware and their current validation limits.

## Features in this release

- Pebble-inspired word clock face and apps for calendar, agenda, tasks, hotspot
  setup, status, notifications, calls, and Now Playing.
- 200×200 monochrome e-paper UI with partial refresh support and host-rendered
  ImageMagick previews for notification, call, and media screens.
- DS3231 timekeeping with configurable timezone/format, Apple BLE Current Time
  Service preference, and Wi-Fi SNTP/HTTP Date fallback.
- CalDAV agenda and task sync, local task completion, and cached organizer data.
- Apple ANCS notifications and caller details, supported notification actions,
  plus AMS track metadata, playback state, and media controls.
- Separate `IBluetooth` transport and `ICompanionSource` provider contracts.
  Apple is implemented; Android and Linux MPRIS remain future integrations.
- Inactivity-based automatic light sleep, ESP-IDF DVFS scopes, ADC-based
  battery estimation, and low-battery handling. Runtime and sleep residency
  still need current-measurement validation on hardware.
- USB `ewctl` status and log queries, plus volatile CPU clock pinning for
  development experiments. Rich output is the default, with `--json` for
  scripts; log tailing batches records to reduce USB round trips.
- Arch Linux package for `ewctl`, with tagged-release builds publishing the
  package and pacman repository database to GitHub Pages and as release assets.

## CPU frequency test commands

```sh
python3 scripts/ewctl.py --port /dev/ttyACM0 power cpu-freq-get
python3 scripts/ewctl.py --port /dev/ttyACM0 power cpu-freq-set 40
python3 scripts/ewctl.py --port /dev/ttyACM0 power cpu-freq-set 0
```

Supported forced frequencies are 40, 80, and 160 MHz. `0` restores automatic
40–160 MHz scaling. `cpu-freq-get` reports the measured frequency; the override
clears at reboot. Test at 40 MHz with BLE/Wi-Fi behavior in mind.

## Firmware assets

- `firmware.bin` — routine app update that preserves saved settings.
- `ewp-factory.bin` — factory image that resets saved settings.
- `SHA256SUMS` — checksums for the firmware files.
- `FLASHING.md` — flashing and image-selection instructions.
- `ewctl-*.pkg.tar.zst`, `ewctl.db`, and `ewctl.files` — Arch package and
  pacman repository index.
