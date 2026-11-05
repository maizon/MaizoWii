#!/usr/bin/env python3
"""Exercise the real scanner's six-character Wii disc-header reader on host."""
from __future__ import annotations
import ctypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class DiscId6(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which("gcc") or shutil.which("cc")
        if not cc:
            raise unittest.SkipTest("host GCC unavailable")
        cls.tmp=tempfile.TemporaryDirectory()
        cls.so=Path(cls.tmp.name)/"wiidiscs.so"
        subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
                        "-D_DEFAULT_SOURCE","-shared","-fPIC",
                        "-I",str(ROOT/"include"),
                        str(ROOT/"source/wii_id.c"),"-o",str(cls.so)],check=True)
        cls.lib=ctypes.CDLL(str(cls.so))
        cls.lib.maizo_wii_id_from_file.argtypes=[ctypes.c_char_p,ctypes.c_void_p]
        cls.lib.maizo_wii_id_from_file.restype=ctypes.c_int
        cls.lib.maizo_wii_id_valid.argtypes=[ctypes.c_char_p]
        cls.lib.maizo_wii_id_valid.restype=ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def probe(self,name,raw):
        f=Path(self.tmp.name)/name
        f.write_bytes(raw)
        result=ctypes.create_string_buffer(7)
        ok=self.lib.maizo_wii_id_from_file(str(f).encode(),result)
        return ok,result.value

    def test_iso_id_at_start(self):
        self.assertEqual(self.probe("Game.iso",b"RMCP01"+bytes(600)),(1,b"RMCP01"))

    def test_wbfs_disc_header_at_offset_0x200(self):
        header=b"WBFS"+bytes(0x200-4)
        self.assertEqual(self.probe("Game.wbfs",header+b"RMCP01"+bytes(50)),(1,b"RMCP01"))

    def test_wbfs_does_not_use_container_magic_as_id(self):
        self.assertEqual(self.probe("Game.wbfs",b"WBFS"+bytes(600)),(0,b""))

    def test_rejects_unsupported_and_corrupt_header(self):
        self.assertEqual(self.probe("notawii.wbfs",b"NOT!"+bytes(508)+b"RMCP01"),(0,b""))
        self.assertEqual(self.probe("game.wbfs",b"WBFS"+bytes(2)),(0,b""))
        self.assertEqual(self.probe("other.chd",b"RMCP01"+bytes(600)),(0,b""))

    def test_uppercase_alphanumeric_only(self):
        self.assertEqual(self.lib.maizo_wii_id_valid(b"RMCP01"),1)
        for bad in (b"",b"RMCP0",b"rmcp01",b"RMCP-1",b"RMCP012"):
            with self.subTest(bad=bad):
                self.assertEqual(self.lib.maizo_wii_id_valid(bad),0)

class WiiRecoverySourceGuards(unittest.TestCase):
    def source(self,path):
        return (ROOT/path).read_text(encoding="utf-8")
    def test_hid_not_blocked_by_unmounted_mass_storage(self):
        source=self.source("source/input.c")
        self.assertIn("!skip_hid && input_frames>90",source)
        self.assertNotIn("storage_permits_hid",source)
    def test_usb_retry_and_header_reader(self):
        main=self.source("source/main.c")
        lib=self.source("source/library.c")
        self.assertIn("retry_missing_storage();",main)
        self.assertIn("mount_storage(1)",main)
        self.assertIn("maizo_wii_id_from_file(path,g->id)",lib)
    def test_vi_geometry_changes_active_output_not_just_canvas(self):
        video=self.source("source/video_sw.c")
        self.assertIn("mode->viWidth=",video)
        self.assertIn("mode->viXOrigin=",video)
        self.assertIn("VIDEO_Configure(mode);",video)
    def test_three_shared_home_and_game_layouts(self):
        ui=self.source("source/ui.c")
        cfg=self.source("source/config.c")
        self.assertIn('"GRADE","LISTA","CARROSSEL"',ui)
        self.assertIn("case PAGE_HOME:catalog(u,l,c,1)",ui)
        self.assertIn("case PAGE_LIBRARY:catalog(u,l,c,0)",ui)
        self.assertIn("c->layout=val%3;",cfg)
        self.assertNotIn("static void sidebar(",ui)

if __name__=="__main__":
    unittest.main()
