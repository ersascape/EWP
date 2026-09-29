#!/usr/bin/env python3
"""Package PlatformIO outputs as an app update and a full factory image."""

from hashlib import sha256
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".pio" / "build" / "ErsaWearable"
OUTPUT = ROOT / ".pio" / "release"
PARTITION_OFFSET = 0x8000
BOOT_APP0_OFFSET = 0xE000
APPLICATION_OFFSET = 0x10000


def main() -> None:
    files = {
        "bootloader.bin": BUILD / "bootloader.bin",
        "partitions.bin": BUILD / "partitions.bin",
        "boot_app0.bin": ROOT / ".pio-core" / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin",
        "firmware.bin": BUILD / "firmware.bin",
    }
    missing = [str(path) for path in files.values() if not path.is_file()]
    if missing:
        raise SystemExit("Missing PlatformIO output(s): " + ", ".join(missing))

    OUTPUT.mkdir(parents=True, exist_ok=True)
    partition_data = files["partitions.bin"].read_bytes()
    app_partitions = [
        (
            int.from_bytes(partition_data[pos + 4 : pos + 8], "little"),
            int.from_bytes(partition_data[pos + 8 : pos + 12], "little"),
        )
        for pos in range(0, len(partition_data) - 31, 32)
        if partition_data[pos : pos + 2] == b"\xaa\x50" and partition_data[pos + 2] == 0
    ]
    app_partition = next((entry for entry in app_partitions if entry[0] == APPLICATION_OFFSET), None)
    if app_partition is None:
        raise SystemExit(
            f"Expected an app partition at {APPLICATION_OFFSET:#x}; found {app_partitions!r}"
        )
    if files["firmware.bin"].stat().st_size > app_partition[1]:
        raise SystemExit("Firmware image exceeds the app partition size")

    (OUTPUT / "firmware.bin").write_bytes(files["firmware.bin"].read_bytes())

    end = APPLICATION_OFFSET + files["firmware.bin"].stat().st_size
    image = bytearray(b"\xff") * end
    image[: files["bootloader.bin"].stat().st_size] = files["bootloader.bin"].read_bytes()
    image[PARTITION_OFFSET : PARTITION_OFFSET + len(partition_data)] = partition_data
    boot_app0 = files["boot_app0.bin"].read_bytes()
    image[BOOT_APP0_OFFSET : BOOT_APP0_OFFSET + len(boot_app0)] = boot_app0
    app = files["firmware.bin"].read_bytes()
    image[APPLICATION_OFFSET : APPLICATION_OFFSET + len(app)] = app
    (OUTPUT / "ewp-factory.bin").write_bytes(image)

    names = ("firmware.bin", "ewp-factory.bin")
    (OUTPUT / "SHA256SUMS").write_text(
        "".join(f"{sha256((OUTPUT / name).read_bytes()).hexdigest()}  {name}\n" for name in names),
        encoding="ascii",
    )
    (OUTPUT / "FLASHING.md").write_text(
        """# Ersa Wearable firmware images

- `firmware.bin` is an application image for updating a watch with the matching
  bootloader and partition table via the ROM bootloader. Its app offset is
  `0x10000`; the current partition table has only one app slot, so this image
  does not support safe over-the-air updates or rollback.
- `ewp-factory.bin` is a merged ESP32-C3 image for factory flashing at offset
  `0x0`. It includes the bootloader at `0x0`, the partition table at `0x8000`,
  the OTA boot data at `0xE000`, and the application at `0x10000`. Factory
  flashing erases existing settings.
- `SHA256SUMS` contains SHA-256 checksums for all images.

Example factory flash with esptool:

```sh
esptool.py --chip esp32c3 --port PORT write_flash 0x0 ewp-factory.bin
```

Replace `PORT` with the serial port for the watch. Check the image checksum
before flashing. The regular development upload command is `make firmware`.
""",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
