#!/usr/bin/env python3
"""No network: synthetic, strictly-formed DOL and ZIP security tests."""
import struct
import tempfile
import unittest
from pathlib import Path
from package_all import valid_dol, choose_dol, safe_extract

def fake_dol():
    words = [0] * 57
    words[0] = 0x100
    words[18] = 0x80004000
    words[36] = 16
    words[56] = 0x80004000
    return struct.pack(">57I", *words) + bytes(0x100 - 57 * 4) + b"\x00" * 16

class BundleTests(unittest.TestCase):
    def test_dol_valid(self):
        self.assertTrue(valid_dol(fake_dol()))
        self.assertFalse(valid_dol(b"junk" * 64))
        self.assertFalse(valid_dol(fake_dol()[:-1]))

    def test_choose_homebrew_not_forwarder(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            (p / "apps/WiiMC").mkdir(parents=True)
            (p / "apps/WiiMC/boot.dol").write_bytes(fake_dol())
            (p / "channel-forwarder").mkdir()
            (p / "channel-forwarder/boot.dol").write_bytes(fake_dol())
            self.assertEqual(choose_dol(p, "WiiMC").parent.name, "WiiMC")

    def test_zip_traversal_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            with zipfile.ZipFile(path / "sample.zip", "w") as z:
                z.writestr("../escape.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(path / "sample.zip", path / "unpacked")

if __name__ == "__main__":
    unittest.main()
