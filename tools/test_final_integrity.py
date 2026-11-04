#!/usr/bin/env python3
"""Regression tests for final executable integrity, including fallback binaries."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from overlay_adapted import overlay
from test_bundle import fake_dol

TOOLS = Path(__file__).resolve().parent


class FinalDolIntegrityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.dist = self.root / "dist"
        self.patched = self.root / "patched"
        self.modules = ("maizo", "usbloader_gx", "Nintendont", "WiiSXRX",
                        "snes9xgx", "fceugx", "vbagx", "WiiMC")
        for name in self.modules:
            dest = self.dist / "apps" / name
            dest.mkdir(parents=True)
            (dest / "boot.dol").write_bytes(fake_dol())
        for rel in ("apps/maizo/meta.xml", "controllers/0079_0006.ini",
                    "maizo/controllers/0079_0006.ini"):
            target = self.dist / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("test", encoding="utf-8")
        manifest = {"upstream": {n: {"tag": "test"} for n in self.modules}}
        target = self.dist / "maizo/MODULE_MANIFEST.json"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(manifest), encoding="utf-8")

    def verify(self):
        return subprocess.run(
            [sys.executable, str(TOOLS / "verify_bundle.py"), str(self.dist)],
            capture_output=True, text=True, check=False)

    def test_fallback_modules_are_hashed(self):
        overlay(self.patched, self.dist)
        self.assertEqual(self.verify().returncode, 0, self.verify().stderr)
        manifest = json.loads((self.dist / "maizo/MODULE_MANIFEST.json").read_text())
        self.assertEqual(set(manifest["final_dol_sha256"]), set(self.modules))

    def test_additional_forwarder_and_installer_are_hashed(self):
        import hashlib
        for name in ("maizo-forwarder", "maizo-installer"):
            folder = self.dist / "apps" / name
            folder.mkdir(parents=True)
            (folder / "boot.dol").write_bytes(fake_dol())
            (folder / "meta.xml").write_text("<app/>")
        (self.dist / "maizo/YAWM-LICENSE").write_text("GPL-2.0 test fixture")
        (self.dist / "wad").mkdir()
        (self.dist / "wad/wm_config.txt").write_text("StartupPath=/wad\n")
        (self.dist / "wad/LEIA-ANTES-DE-INSTALAR.txt").write_text("test")
        overlay(self.patched, self.dist)
        path = self.dist / "maizo/MODULE_MANIFEST.json"
        manifest = json.loads(path.read_text())
        digest = hashlib.sha256(fake_dol()).hexdigest()
        manifest["forwarder"] = {"path": "apps/maizo-forwarder/boot.dol",
                                 "sha256": digest}
        manifest["installer"] = {
            "source": "modmii/YAWM-ModMii-Edition",
            "auto_installs": False, "channel_wad_included": False,
            "dol_sha256": digest,
        }
        path.write_text(json.dumps(manifest))
        self.assertEqual(self.verify().returncode, 0, self.verify().stderr)
        (self.dist / "apps/maizo-installer/boot.dol").write_bytes(fake_dol() + b"bad")
        self.assertIn("installer hash mismatch", self.verify().stderr)
        (self.dist / "apps/maizo-installer/boot.dol").write_bytes(fake_dol())
        (self.dist / "maizo/YAWM-LICENSE").unlink()
        self.assertIn("installer GPL license missing", self.verify().stderr)

    def test_hid_installer_overlay_hash_and_fallback(self):
        import hashlib
        installer = self.dist / "apps/maizo-installer"
        installer.mkdir(parents=True)
        (installer / "boot.dol").write_bytes(fake_dol())
        (installer / "meta.xml").write_text("<app/>")
        manifest_file = self.dist / "maizo/MODULE_MANIFEST.json"
        manifest = json.loads(manifest_file.read_text())
        manifest["installer"] = {
            "source": "modmii/YAWM-ModMii-Edition",
            "auto_installs": False, "channel_wad_included": False,
            "dol_sha256": hashlib.sha256(fake_dol()).hexdigest()
        }
        manifest_file.write_text(json.dumps(manifest))
        (self.dist / "maizo/YAWM-LICENSE").write_text("GPL test")
        wad = self.dist / "wad"
        wad.mkdir()
        (wad / "LEIA-ANTES-DE-INSTALAR.txt").write_text("test")
        (wad / "wm_config.txt").write_text("StartupPath=/wad\n")
        # Uncompiled adapter must never be represented as HID-enabled.
        overlay(self.patched, self.dist)
        got = json.loads(manifest_file.read_text())
        self.assertIs(got["installer"]["hid_adapted"], False)
        self.assertEqual(self.verify().returncode, 0, self.verify().stderr)
        candidate = self.patched / "apps/maizo-installer"
        candidate.mkdir(parents=True)
        dol = fake_dol()[:-16] + bytes([0x51])*16
        (candidate / "boot.dol").write_bytes(dol)
        overlay(self.patched, self.dist)
        got = json.loads(manifest_file.read_text())
        self.assertIs(got["installer"]["hid_adapted"], True)
        self.assertEqual(got["installer"]["dol_sha256"],
                         hashlib.sha256(dol).hexdigest())
        self.assertEqual(self.verify().returncode, 0, self.verify().stderr)
        # A tampered installer must fail validation rather than ship.
        (installer / "boot.dol").write_bytes(fake_dol())
        self.assertIn("installer hash mismatch", self.verify().stderr)

    def test_corruption_is_rejected(self):
        overlay(self.patched, self.dist)
        dol = self.dist / "apps/WiiMC/boot.dol"
        dol.write_bytes(dol.read_bytes() + b"unexpected")
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("DOL SHA-256 mismatch: WiiMC", result.stderr)

    def test_missing_hash_is_rejected(self):
        overlay(self.patched, self.dist)
        target = self.dist / "maizo/MODULE_MANIFEST.json"
        manifest = json.loads(target.read_text())
        del manifest["final_dol_sha256"]["maizo"]
        target.write_text(json.dumps(manifest))
        self.assertNotEqual(self.verify().returncode, 0)


if __name__ == "__main__":
    unittest.main()
