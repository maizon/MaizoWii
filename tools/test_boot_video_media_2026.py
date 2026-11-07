#!/usr/bin/env python3
"""Release regression checks. These do not replace tests on a physical Wii."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def read(p): return (ROOT/p).read_text(encoding="utf-8")

class BootVideoMediaRegressions(unittest.TestCase):
    def test_priiloader_autoboot_starts_before_storage_mounted(self):
        s=read("source/main.c")
        body=s[s.index("static int mount_storage(int initial){"):s.index("/* Forwarder -> DOL handoff")]
        self.assertIn("__io_usbstorage.startup()",body)
        self.assertIn("__io_usbstorage.isInserted()",body)
        self.assertIn('fatMountSimple("usb",&__io_usbstorage)',body)
        self.assertIn("__io_wiisd.isInserted()",body)
        self.assertNotIn("fatInitDefault()",body)
        self.assertIn('ui_boot_message(n==0?',body)
        self.assertIn("retry_missing_storage();",s)
        self.assertIn("state.frame%300",s)
    def test_nintendont_auto_boot_arguments_and_independent_config(self):
        launch=read("source/launch.c")
        upstream_contract=read("source/launch_args.c")
        self.assertIn("NIN_CFG_AUTO_BOOT",launch)
        self.assertIn("memcpy(command.data+command.length,&nin,sizeof(nin))",launch)
        self.assertIn("(void)save_nincfg(g,c,&nin);",launch)
        self.assertIn("maizo_gc_relative_path",launch)
        self.assertIn("n.VideoScale=(s8)scale",launch)
        self.assertIn("n.VideoOffset=(s8)x",launch)
        self.assertIn('NIN_CFG_VERSION 0x0000000Bu',launch)
        self.assertIn("MAIZO_ARG_GC",upstream_contract)
        self.assertNotIn("POSICAO VERTICAL GC",read("source/settings.c"))
    def test_game_video_240p_vi_is_centered_once(self):
        video=read("integrations/maizo_game_video.inc")
        patches=read("tools/patch_upstreams.py")
        launch=read("source/launch.c")
        self.assertIn('maizo_video_adjust_vi(rmode,1)',patches)
        self.assertIn("EmuSettings.videoMode == VIDEOMODE_ORIGINAL_240P?0:cal.x",patches)
        self.assertIn("mode->viXOrigin=(u16)x",video)
        self.assertIn("video_w*c->game_w240",launch)
        self.assertIn("video_x-50+(int)c->game_x240-50",launch)
        self.assertIn("game_y240=%d",launch)
        self.assertIn("video_y-50",launch)
    def test_wiimc_iso_and_options_bridge_are_real(self):
        lib=read("source/library.c")
        settings=read("source/settings.c")
        cfg=read("source/config.c")
        launch=read("source/launch.c")
        bridge=read("integrations/maizo_wiimc_settings.inc")
        patches=read("tools/patch_upstreams.py")
        options=read("include/wiimc_options.h")
        self.assertIn('".vob",".iso",".wmv"',lib)
        self.assertIn("MAIZO_WIIMC_OPTION_COUNT 35",options)
        self.assertIn('wiimc_apply',cfg)
        self.assertIn('wiimc_apply=1',launch)
        self.assertIn("if(!correct_system||!correct_device||!enabled)continue",bridge)
        self.assertNotIn("MaizoForceVI704",bridge)
        self.assertIn("maizo_wiimc_apply_profile();",patches)
        self.assertIn('patch_file(root, "source/settings.cpp", wiimc_settings)',patches)
        self.assertIn("MAIZO_WIIMC_OPTION_COUNT",settings)
    def test_unverified_wad_not_claimed_stable(self):
        workflow=read(".github/workflows/build.yml")
        self.assertIn("ALL-IN-ONE",workflow)
        self.assertNotIn("MAIZO-WAD-AUTOINSTALL",workflow)

if __name__=="__main__":unittest.main()
