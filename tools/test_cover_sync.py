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

    def test_wii_wbfs_disc_id_from_real_offset(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            p=root/"No ID in name.wbfs"
            p.write_bytes(b"WBFS"+bytes(0x1fc)+b"RZDE01"+bytes(64))
            self.assertEqual(list(mod.images(root,"wii")),["RZDE01"])

    def test_wbfs_invalid_container_does_not_yield_fake_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"Bad image.wbfs"
            p.write_bytes(b"NOPE"+bytes(0x1fc)+b"ABCD12")
            self.assertEqual(list(mod.images(Path(tmp),"wii")),[])

    def test_gamecube_ciso_header_not_misidentified_as_disc_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"game.ciso"
            p.write_bytes(b"CISO00"+bytes(1024))
            self.assertEqual(list(mod.images(Path(tmp),"gc")),[])

    def test_libretro_game_title_and_gx_key(self):
        self.assertEqual(mod.cover_key("Super Mario World (USA)"),
                         "SUPER-MARIO-WORLD-USA")
        self.assertEqual(mod.titles_for_lookup("Super_Mario_World (PT-BR)")[1],
                         "Super Mario World")

    def test_non_disc_catalog(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"snes9xgx/roms/Super Mario World (USA).sfc"
            p.parent.mkdir(parents=True)
            p.write_bytes(b"not a commercial game image")
            games=list(mod.named_roms(Path(tmp),"snes"))
            self.assertEqual(games,[("SUPER-MARIO-WORLD-USA","Super Mario World (USA)")])

if __name__ == "__main__":
    unittest.main()
