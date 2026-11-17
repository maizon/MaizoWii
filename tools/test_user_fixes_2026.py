"""Source-level regressions for the 17 Nov 2026 Wii hardware reports.
These are NOT proof of Nintendont launch, analog output or USB hotplug on Wii.
"""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def read(name):return (ROOT/name).read_text(encoding="utf-8")

class UserHardwareFixContracts(unittest.TestCase):
 def test_gamecube_is_a_home_console_icon(self):
  icon=read("tools/prepare_system_icons.py")
  self.assertIn('if name == "IGC":',icon)
  self.assertIn('for code in ("IWII", "IGC", "IHB", *ICONS):',icon)
  self.assertNotIn('"IGC": "NGC"',icon)
  self.assertIn('"IGC"',read("source/main.c"))

 def test_hotplug_detects_removal_even_with_both_mounted(self):
  main=read("source/main.c")
  self.assertIn("const int lost_usb=storage_usb&&!__io_usbstorage.isInserted();",main)
  self.assertIn('fatUnmount("usb");',main)
  self.assertIn("__io_usbstorage.shutdown();",main)
  self.assertIn("mount_storage(0);",main)
  self.assertIn("library_end(&library_state);",main)
  self.assertIn("module_refresh_available();",main)

 def test_nintendont_persists_settings_and_checks_autoboot(self):
  launch=read("source/launch.c")
  for symbol in ("nincfg.bin","remove(backup);",
                 "memcmp(&stored,n,sizeof(stored))","NIN_CFG_AUTO_BOOT",
                 "nin.Magicbytes!=NIN_CFG_MAGIC"):
   self.assertIn(symbol,launch)
  self.assertIn("memcpy(command.data+command.length,&nin,sizeof(nin))",launch)

 def test_pal_m_uses_modified_wiimc_not_original(self):
  launch=read("source/launch.c")
  patch=read("tools/patch_upstreams.py")
  self.assertIn("!maizo_effective_tv_standard(c,SYSTEM_MEDIA)",launch)
  for marker in ("WiiMC preserve PAL-M leaving double strike",
                 "WiiMC 240p standard selection",
                 "WiiMC interlaced video selection"):
   self.assertIn(marker,patch)

 def test_download_is_resumable_after_indexing_and_aspect_correct(self):
  cover=read("source/cover_download.c")
  self.assertIn("wait_for_index=1;job.active=1",cover)
  self.assertIn("network_retry_frame++%120u",cover)
  self.assertIn("save_png_as_gxt(&received,current_dest,current_game.system)",cover)
  self.assertIn("system==SYSTEM_PS1?128u:system==SYSTEM_GB?160u:ART_HEIGHT",cover)
  for symbol in ("128&&hdr[7]!=160&&hdr[7]!=176", "st.st_size!=8+128*128*4"):
   self.assertIn(symbol,read("source/covers.c"))
  self.assertIn("height_for_system",read("tools/build_cover_cache.py"))

 def test_crt_selection_draws_true_curved_corners(self):
  ui=read("source/ui.c")
  self.assertIn("for(int row=0;row<r;row++)",ui)
  self.assertIn("dx*dx+dy*dy>r*r",ui)
  self.assertIn("rounded(x-5,y-5,w+10,h+10,focus_color)",ui)

if __name__=="__main__":unittest.main()
