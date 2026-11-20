#!/usr/bin/env python3
"""Host-compiled cartridge/disc identity extraction tests; no Wii needed."""
import ctypes
import pathlib
import shutil
import struct
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]

class IdentityProbeTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cc=shutil.which("gcc") or shutil.which("cc")
  if not cc:raise unittest.SkipTest("No host C compiler")
  cls.tmp=tempfile.TemporaryDirectory()
  lib=pathlib.Path(cls.tmp.name)/"probe.so"
  subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror","-shared","-fPIC",
    "-I",str(ROOT/"include"),str(ROOT/"source/game_identity.c"),
    "-o",str(lib)],check=True,capture_output=True)
  cls.probe=ctypes.CDLL(str(lib)).maizo_game_probe_identity
  cls.probe.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_void_p,
                      ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t]
  cls.probe.restype=ctypes.c_int
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def run_probe(self,name,sys,contents):
  path=pathlib.Path(self.tmp.name)/name
  path.write_bytes(contents)
  prod=ctypes.create_string_buffer(16);title=ctypes.create_string_buffer(112)
  ok=self.probe(str(path).encode(),sys,prod,16,title,112)
  return ok,prod.value.decode(),title.value.decode()
 def test_wii_id_from_disc_not_filename(self):
  h=bytearray(0x80);h[:6]=b"RMCE01";h[0x18:0x1c]=b"\x5d\x1c\x9e\xa3"
  h[0x20:0x2a]=b"Mario Kart"
  ok,code,title=self.run_probe("FAKE01.iso",0,h)
  self.assertEqual((ok,code,title),(1,"RMCE01","Mario Kart"))
  w=bytearray(0x200+len(h));w[:4]=b"WBFS";w[0x200:]=h
  self.assertEqual(self.run_probe("foo.wbfs",0,w)[1],"RMCE01")
 def test_gamecube_and_ciso_header(self):
  h=bytearray(0x80);h[:6]=b"GZLE01";h[0x1c:0x20]=b"\xc2\x33\x9f\x3d"
  h[0x20:0x2b]=b"THE LEGEND "
  self.assertEqual(self.run_probe("game.iso",1,h)[1],"GZLE01")
  c=bytearray(0x8000+len(h));c[:4]=b"CISO";c[4:8]=struct.pack("<I",0x200000)
  c[8]=1;c[0x8000:]=h
  self.assertEqual(self.run_probe("game.ciso",1,c)[1],"GZLE01")
  self.assertEqual(self.run_probe("no.iso",1,b"CISO"+b"\\0"*400)[0],0)
 def test_gba_product_code_and_title(self):
  h=bytearray(0xc0);h[0xa0:0xa7]=b"POKEMON";h[0xac:0xb0]=b"BPEE";h[0xb2]=0x96
  self.assertEqual(self.run_probe("Pokemon.gba",5,h),(1,"BPEE","POKEMON"))
 def test_gb_title_is_not_a_fake_product_code(self):
  h=bytearray(0x150);h[0x104:0x108]=b"\xce\xed\x66\x66"
  h[0x134:0x13b]=b"POKEMON"
  self.assertEqual(self.run_probe("Crystal.gbc",6,h),(1,"","POKEMON"))
 def test_snes_title_checked_before_matching(self):
  h=bytearray(0x8000);h[0x7fc0:0x7fd1]=b"SUPER MARIO WORLD"
  h[0x7fdc:0x7fde]=struct.pack("<H",0xedcb)
  h[0x7fde:0x7fe0]=struct.pack("<H",0x1234)
  self.assertEqual(self.run_probe("Mario.sfc",3,h),(1,"","SUPER MARIO WORLD"))
 def test_invalid_must_not_claim_game_id(self):
  self.assertEqual(self.run_probe("game.iso",1,b"\x00"*128)[0],0)
  self.assertEqual(self.run_probe("file.gba",5,b"\x00"*192)[0],0)

class IntegrationContracts(unittest.TestCase):
 def get(self,path):return (ROOT/path).read_text(encoding="utf-8")
 def test_synchronize_nintendont_on_save_and_preserve_unknown_flags(self):
  launch=self.get("source/launch.c")
  main=self.get("source/main.c")
  for symbol in ("maizo_build_nincfg","maizo_read_nincfg_root",
                 "n.Config &= ~owned","int module_sync_nintendont",
                 "maizo_write_nincfg_root", "if(autoboot)n.Config|=NIN_CFG_AUTO_BOOT"):
   self.assertIn(symbol,launch)
  self.assertIn("module_sync_nintendont(&state.game_config,&library_state.games[idx])",main)
  self.assertIn("module_sync_nintendont(&config,NULL)",main)
  self.assertIn("c->theme=5",self.get("source/config.c"))
 def test_wiimc_usb_hid_does_not_disable_when_usb_disk_attached(self):
  patch=self.get("tools/patch_upstreams.py")
  self.assertIn("HID class scan even with USB disk mounted",patch)
  self.assertIn("else maizo_hid_tick(); /* USB disk and HID are independent */",patch)
  self.assertNotIn("if(!isInserted[DEVICE_USB])",patch)
  self.assertNotIn("wiiIsPlaying() || isInserted[DEVICE_USB]",patch)
 def test_original_disc_id_prioritized_and_libretro_mirror_reached(self):
  lib=self.get("source/library.c")
  dl=self.get("source/cover_download.c")
  self.assertIn("maizo_game_probe_identity",lib)
  self.assertIn("g->id[0]=0;",lib)
  self.assertIn("g->lookup_title",dl)
  self.assertIn("current_game.product_code",dl)
  self.assertIn("requested>=24",dl)
  self.assertIn("raw.githubusercontent.com/libretro-thumbnails",dl)
  self.assertIn("REDIRECIONOU - TENTANDO LIBRETRO",dl)

if __name__=="__main__":unittest.main()
