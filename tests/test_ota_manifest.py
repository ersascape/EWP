import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from create_ota_manifest import create_manifest


class OtaManifestTests(unittest.TestCase):
    def test_manifest_has_release_url_size_and_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            payload = b"firmware test image"
            image.write_bytes(payload)

            manifest = create_manifest(image, "ewp-0.1.3")

        self.assertEqual(manifest["schema"], 1)
        self.assertEqual(manifest["tag"], "ewp-0.1.3")
        self.assertEqual(manifest["version"], "ewp-0.1.3")
        self.assertEqual(manifest["firmware_url"],
                         "https://pkgs-wearables.ersa.dev/firmware/ewp-0.1.3.bin")
        self.assertEqual(manifest["size"], len(payload))
        self.assertEqual(manifest["sha256"], hashlib.sha256(payload).hexdigest())

    def test_manifest_rejects_non_ewp_tags(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            image.write_bytes(b"image")
            with self.assertRaises(ValueError):
                create_manifest(image, "v0.1.3")


if __name__ == "__main__":
    unittest.main()
