"""Regression: a second packaging pass must not advertise stale HID binaries."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "overlay_adapted.py"
MODULES = ("maizo", "usbloader_gx", "Nintendont", "WiiSXRX", "WiiSX",
           "snes9xgx", "fceugx", "vbagx", "WiiMC")

class OverlayProvenanceTest(unittest.TestCase):
    def test_second_pass_clears_stale_adapter_hash(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            dist = root / "dist"
            manifest = dist / "maizo" / "MODULE_MANIFEST.json"
            manifest.parent.mkdir(parents=True)
            manifest.write_text(json.dumps({"adapted_sha256": {"WiiMC": "stale"}}))
            for name in MODULES:
                dest = dist / "apps" / name / "boot.dol"
                dest.parent.mkdir(parents=True)
                dest.write_bytes(b"official-" + name.encode())
            stub = types.ModuleType("package_all")
            stub.valid_dol = lambda data: True
            with patch.dict(sys.modules, {"package_all": stub}):
                spec = importlib.util.spec_from_file_location("maizo_overlay_test", SCRIPT)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                module.overlay(root / "no-adapters", dist)
            result = json.loads(manifest.read_text())
            self.assertEqual(result["adapted_sha256"], {})
            self.assertEqual(result["adapted_modules"], [])
            self.assertEqual(set(result["fallback_modules"]), set(module.MODULES))
            self.assertIn("WiiMC", result["final_dol_sha256"])

    def test_rerun_refuses_to_relabel_existing_adapted_binary(self):
        import hashlib
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            dist = root / "dist"
            manifest = dist / "maizo" / "MODULE_MANIFEST.json"
            manifest.parent.mkdir(parents=True)
            for name in MODULES:
                dest = dist / "apps" / name / "boot.dol"
                dest.parent.mkdir(parents=True)
                dest.write_bytes(b"official-" + name.encode())
            adapted = dist / "apps" / "WiiMC" / "boot.dol"
            adapted.write_bytes(b"previously-adapted")
            manifest.write_text(json.dumps({"adapted_sha256": {
                "WiiMC": hashlib.sha256(adapted.read_bytes()).hexdigest()
            }}))
            stub = types.ModuleType("package_all")
            stub.valid_dol = lambda data: True
            with patch.dict(sys.modules, {"package_all": stub}):
                spec = importlib.util.spec_from_file_location("maizo_overlay_test", SCRIPT)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                with self.assertRaisesRegex(ValueError, "rebuild clean dist"):
                    module.overlay(root / "no-adapters", dist)
            self.assertEqual(adapted.read_bytes(), b"previously-adapted")

    def test_invalid_later_adapter_does_not_modify_earlier_module(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            dist = root / "dist"
            manifest = dist / "maizo" / "MODULE_MANIFEST.json"
            manifest.parent.mkdir(parents=True)
            manifest.write_text(json.dumps({"adapted_sha256": {}}))
            for name in MODULES:
                dest = dist / "apps" / name / "boot.dol"
                dest.parent.mkdir(parents=True)
                dest.write_bytes(b"official-" + name.encode())
            first = root / "adapters" / "apps" / "usbloader_gx" / "boot.dol"
            later = root / "adapters" / "apps" / "WiiSXRX" / "boot.dol"
            first.parent.mkdir(parents=True)
            later.parent.mkdir(parents=True)
            first.write_bytes(b"valid-adapter")
            later.write_bytes(b"corrupt-adapter")
            stub = types.ModuleType("package_all")
            stub.valid_dol = lambda data: data == b"valid-adapter"
            with patch.dict(sys.modules, {"package_all": stub}):
                spec = importlib.util.spec_from_file_location("maizo_overlay_atomic_test", SCRIPT)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                with self.assertRaisesRegex(ValueError, "Invalid adapted module: WiiSXRX"):
                    module.overlay(root / "adapters", dist)
            self.assertEqual((dist / "apps" / "usbloader_gx" / "boot.dol").read_bytes(),
                             b"official-usbloader_gx")
            self.assertEqual(json.loads(manifest.read_text()), {"adapted_sha256": {}})

if __name__ == "__main__":
    unittest.main()
