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
