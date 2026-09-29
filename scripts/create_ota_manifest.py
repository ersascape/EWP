#!/usr/bin/env python3
"""Create the small OTA manifest published alongside each firmware release."""

import argparse
import hashlib
import json
import re
from pathlib import Path


def create_manifest(image: Path, tag: str) -> dict:
    if not re.fullmatch(r"ewp-[A-Za-z0-9._-]+", tag):
        raise ValueError("release tag must start with ewp-")
    payload = image.read_bytes()
    return {
        "schema": 1,
        "tag": tag,
        "version": tag,
        "firmware_url": f"https://pkgs-wearables.ersa.dev/firmware/{tag}.bin",
        "sha256": hashlib.sha256(payload).hexdigest(),
        "size": len(payload),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    manifest = create_manifest(args.firmware, args.tag)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
