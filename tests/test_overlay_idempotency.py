"""Regression: a stale manifest must not claim a missing HID adapter."""
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from overlay_adapted import MODULES, overlay


class OverlayProvenanceTests(unittest.TestCase):
    def test_stale_adapter_hash_is_removed_on_fresh_dist(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            dist = root / "dist"
            patched = root / "patched"
            patched.mkdir()
            (dist / "maizo").mkdir(parents=True)
            for name in ("maizo", "usbloader_gx", "Nintendont", "WiiSXRX",
                         "snes9xgx", "fceugx", "vbagx", "WiiMC"):
                app = dist / "apps" / name
                app.mkdir(parents=True)
                (app / "boot.dol").write_bytes(b"official " + name.encode())
            manifest = {
                "adapted_sha256": {"WiiMC": "stale-digest"},
                "module_input_provenance": {"WiiMC": "maizo HID source adapter"},
            }
            (dist / "maizo" / "MODULE_MANIFEST.json").write_text(
                json.dumps(manifest), encoding="utf-8")
            overlay(patched, dist)
            actual = json.loads((dist / "maizo" / "MODULE_MANIFEST.json").read_text())
            self.assertEqual(actual["adapted_sha256"], {})
            self.assertEqual(actual["adapted_modules"], [])
            self.assertEqual(set(actual["fallback_modules"]), set(MODULES))
            self.assertNotIn("HID source adapter", actual["module_input_provenance"]["WiiMC"])
            self.assertIn("WiiMC", (dist / "HID-COMPATIBILITY.txt").read_text())


if __name__ == "__main__":
    unittest.main()
