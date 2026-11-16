"""Source contract checks for optional TV standards and WiiMC USB/ISO guards.
Compilation and Wii validation are required separately."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(name): return (ROOT/name).read_text(encoding="utf-8")

class VideoStandardContracts(unittest.TestCase):
 def test_tv_modes_saved_per_core_and_per_game(self):
  cfg=read("source/config.c")
  ui=read("source/settings.c")
  self.assertIn("tv_global=",cfg)
  self.assertIn("tv_module_%u=",cfg)
  self.assertIn("tv_module=%u",cfg)
  for option in ("PAL-M","NTSC","PAL50","PAL60","HERDAR GLOBAL"):
   self.assertIn(option,ui)
  self.assertIn("SYSTEM_HB)return -1",ui)
  main=read("source/main.c")
  video=read("source/video_sw.c")
  self.assertIn("maizo_video_frontend_tv_select(config.tv_global)",main)
  self.assertIn("state.tv_preview_pending=0",main)
  self.assertIn("video_mode.viTVMode",video)

 def test_launch_consumed_only_by_patched_engines(self):
  launch=read("source/launch.c")
  self.assertIn("maizo_effective_tv_standard",launch)
  self.assertIn("tv_standard=%u",launch)
  self.assertIn("n.VideoMode=0x00010000u|2u",launch)
  helper=read("integrations/maizo_tv_standard.inc")
  for symbol in ("TVNtsc480IntDf","TVPal576IntDfScale",
                 "TVEurgb60Hz480IntDf","TVMpal480IntDf",
                 "TVMpal240Ds"):
   self.assertIn(symbol,helper)
  self.assertIn("VI_PROGRESSIVE",helper)

 def test_wiimc_usb_browser_iso_guards_and_video(self):
  patch=read("tools/patch_upstreams.py")
  self.assertIn("WiiMC TV mode after mounted FAT",patch)
  self.assertIn("WiiMC IFO filename slash NULL guard",patch)
  self.assertIn("WiiMC browser invalid mount must abort",patch)
  self.assertIn("if(!entry)return browser.numEntries",patch)
  self.assertIn("isInserted[DEVICE_USB]",patch)
  self.assertIn("ChangeInterface(loadedFile,true)",patch)
  self.assertNotIn("WiiMC cap dead startup/unsupported codec",patch)

if __name__=="__main__": unittest.main()
