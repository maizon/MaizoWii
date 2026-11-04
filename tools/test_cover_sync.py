#!/usr/bin/env python3
"""Host-only tests; no network or game images distributed."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("cover_sync", HERE / "cover_sync.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

class CoverTests(unittest.TestCase):
    def test_gamecube_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "Zelda" / "game.iso"
            p.parent.mkdir()
            p.write_bytes(b"GZLE01" + bytes(512))
            self.assertEqual(list(mod.images(Path(tmp), "gc")), ["GZLE01"])

    def test_wii_wbfs_folder_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "Some Game [RSBE01]" / "random.wbfs"
            p.parent.mkdir()
            p.write_bytes(b"WBFS" + bytes(64))
            self.assertEqual(list(mod.images(Path(tmp), "wii")), ["RSBE01"])

    def test_wii_iso_header_and_unique(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / "game1.iso"
            b = Path(tmp) / "game2.iso"
            a.write_bytes(b"RZDE01" + bytes(64))
            b.write_bytes(b"RZDE01" + bytes(64))
            self.assertEqual(list(mod.images(Path(tmp), "wii")), ["RZDE01"])

    def test_invalid_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "bad.iso").write_bytes(b"??????")
            self.assertEqual(list(mod.images(Path(tmp), "gc")), [])

if __name__ == "__main__":
    unittest.main()
