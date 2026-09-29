#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
image_dir="${1:-$repo_root/.pio/release}"
port="${2:-${PORT:-}}"

usage() {
  cat <<'EOF'
Usage: scripts/migrate_ota.sh [release-directory] [serial-port]

Perform the one-time migration from the legacy single-slot partition table to
the dual-slot OTA layout. Requires bootloader.bin, partitions.bin,
boot_app0.bin, firmware.bin, and SHA256SUMS from the same firmware build.
Checks all component hashes before flashing. NVS settings are preserved. This
overwrites the old SPIFFS region, which is unused by EWP.

The serial port may also be set with PORT=/dev/ttyACM0.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

for image in bootloader.bin partitions.bin boot_app0.bin firmware.bin SHA256SUMS; do
  if [[ ! -f "$image_dir/$image" ]]; then
    echo "Required migration image not found: $image_dir/$image" >&2
    exit 2
  fi
done

for image in bootloader.bin partitions.bin boot_app0.bin firmware.bin; do
  expected="$(awk -v name="$image" '$2 == name { print $1; exit }' "$image_dir/SHA256SUMS")"
  if [[ -z "$expected" ]]; then
    echo "No SHA-256 checksum recorded for $image" >&2
    exit 2
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    actual="$(sha256sum "$image_dir/$image" | awk '{ print $1 }')"
  else
    actual="$(shasum -a 256 "$image_dir/$image" | awk '{ print $1 }')"
  fi
  if [[ "$actual" != "$expected" ]]; then
    echo "SHA-256 verification failed for $image" >&2
    exit 2
  fi
done

if [[ -z "$port" ]]; then
  shopt -s nullglob
  ports=(/dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.usbserial*)
  if [[ ${#ports[@]} -eq 1 ]]; then
    port="${ports[0]}"
  else
    echo "Could not choose one serial port automatically." >&2
    printf 'Detected ports: %s\n' "${ports[*]:-(none)}" >&2
    echo "Pass the port as the second argument or set PORT." >&2
    exit 2
  fi
fi

esptool=()
if command -v esptool.py >/dev/null 2>&1; then
  esptool=(esptool.py)
elif command -v esptool >/dev/null 2>&1; then
  esptool=(esptool)
elif [[ -f "$repo_root/.pio-core/packages/tool-esptoolpy/esptool.py" ]]; then
  esptool=(python3 "$repo_root/.pio-core/packages/tool-esptoolpy/esptool.py")
else
  echo "esptool is required (install esptool or build with PlatformIO first)." >&2
  exit 2
fi

echo "Migrating $port to dual-slot OTA layout; keep USB connected until it completes."
echo "Saved Wi-Fi/settings are preserved. The unused old SPIFFS region becomes OTA slot 1."
"${esptool[@]}" --chip esp32c3 --port "$port" --baud 460800 write_flash \
  0x10000 "$image_dir/firmware.bin"
"${esptool[@]}" --chip esp32c3 --port "$port" --baud 460800 write_flash \
  0x8000 "$image_dir/partitions.bin"
"${esptool[@]}" --chip esp32c3 --port "$port" --baud 460800 write_flash \
  0xE000 "$image_dir/boot_app0.bin"
"${esptool[@]}" --chip esp32c3 --port "$port" --baud 460800 write_flash \
  0x0 "$image_dir/bootloader.bin"
