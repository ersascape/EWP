import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from create_ota_manifest import create_manifest, read_device_info, selected_device_info


class OtaManifestTests(unittest.TestCase):
    def test_manifest_has_release_url_size_and_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            payload = b"firmware test image"
            image.write_bytes(payload)

            identity = read_device_info(selected_device_info(Path(__file__).resolve().parents[1]))
            manifest = create_manifest(image, "ewp-0.1.3", identity)

        self.assertEqual(manifest["schema"], 1)
        self.assertEqual(manifest["device_name"], "Ampere Terra")
        self.assertEqual(manifest["codename"], "terra")
        self.assertEqual(manifest["manufacturer"], "Ampere Works")
        self.assertEqual(manifest["tag"], "ewp-0.1.3")
        self.assertEqual(manifest["version"], "ewp-0.1.3")
        self.assertEqual(manifest["firmware_url"],
                         "https://pkgs-wearables.ersa.dev/firmware/terra/ewp-0.1.3.bin")
        self.assertEqual(manifest["size"], len(payload))
        self.assertEqual(manifest["sha256"], hashlib.sha256(payload).hexdigest())

    def test_manifest_rejects_non_ewp_tags(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            image.write_bytes(b"image")
            with self.assertRaises(ValueError):
                create_manifest(image, "v0.1.3", read_device_info(selected_device_info(
                    Path(__file__).resolve().parents[1])))

    def test_manifest_identity_matches_bsp_source(self):
        identity = read_device_info(selected_device_info(Path(__file__).resolve().parents[1]))
        self.assertEqual(identity, {
            "device_name": "Ampere Terra",
            "codename": "terra",
            "manufacturer": "Ampere Works",
        })

    def test_cli_writes_codename_scoped_manifest_and_firmware(self):
        project = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "firmware.bin"
            site = root / "site"
            payload = b"image-bytes"
            image.write_bytes(payload)
            result = subprocess.run(
                [sys.executable, str(project / "scripts/create_ota_manifest.py"),
                 str(image), "--tag", "ewp-0.1.3", "--project-root", str(project),
                 "--site-root", str(site)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest_path = site / "ota/terra/ota.json"
            firmware_path = site / "firmware/terra/ewp-0.1.3.bin"
            self.assertTrue(manifest_path.is_file())
            self.assertEqual(firmware_path.read_bytes(), payload)
            self.assertEqual(json.loads(manifest_path.read_text())["codename"], "terra")


if __name__ == "__main__":
    unittest.main()
