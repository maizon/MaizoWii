#!/usr/bin/env python3
"""Direct launch / premium UI release-candidate regression checks.

These contracts cannot prove physical Wii USB/cIOS, Priiloader cold boot,
GameCube compatibility, or 240p alignment.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

class FinalCandidateContracts(unittest.TestCase):
    def test_wii_gx_uses_both_ios_flags_before_direct_game_id(self):
        build=read("source/launch_args.c")
        self.assertIn('"-ios=%u"',build)
        self.assertIn('"-bootios=%u"',build)
        self.assertIn('"-usbport=%u"',build)
        self.assertLess(build.index('"-ios=%u"'),build.index('"-bootios=%u"'))
        self.assertIn('if(!id6(s->id))',build)
        self.assertIn('s->wii_usb_port>2',build)
        self.assertIn('"AMBAS / AUTO"',read("source/settings.c"))
        self.assertIn('wii_usb_port=val>2?0:val;',read("source/config.c"))

    def test_nintendont_binary_args_and_primary_root_fallback(self):
        launch=read("source/launch.c")
        self.assertIn('NIN_CFG_AUTO_BOOT',launch)
        self.assertIn('memcpy(command.data+command.length,&nin,sizeof(nin))',launch)
        self.assertIn('maizo_write_nincfg_root(&n,(n.Config&NIN_CFG_USB)?"usb":"sd")',launch)
        self.assertIn('maizo_write_nincfg_root(&n,(n.Config&NIN_CFG_USB)?"sd":"usb")',launch)
        self.assertIn('(void)save_nincfg(g,c,&nin);',launch)
        self.assertIn('line[command.length]=0;',launch)
        self.assertIn('args->length=command.length+1;',launch)
        self.assertIn('args->argc=command.argc;',launch)
        self.assertIn('MaizoNinCfgSizeCheck',launch)

    def test_ui_brand_title_and_storage_visible_across_screens(self):
        ui=read("source/ui.c")
        draw=ui.split("void ui_draw(",1)[1].split("void ui_shutdown(",1)[0]
        self.assertIn('section_title(u,l,c,home);',ui)
        self.assertIn('u->storage_usb?"OK":"--"',ui)
        self.assertIn('u->storage_sd?"OK":"--"',ui)
        self.assertIn('void ui_boot_message(',ui)
        self.assertIn('ui_boot_message(stage);',read("source/main.c"))
        self.assertGreater(draw.rfind('brand_overlay();'),draw.rfind('topbar(u,l,hid);'))
        self.assertGreater(draw.rfind('brand_overlay();'),draw.rfind('footer(u);'))
        self.assertLess(draw.rfind('brand_overlay();'),draw.rfind('maizo_video_present();'))
        self.assertNotIn('label(25,23+dy,"MAIZO"',ui)

    def test_wiimc_video_original_not_overridden(self):
        launch=read("source/launch.c")
        self.assertIn('c->force_vi704&&g->system!=SYSTEM_MEDIA',launch)
        patch=read("tools/patch_upstreams.py")
        bridge=read("integrations/maizo_wiimc_settings.inc")
        self.assertNotIn('wiimc_vi704',patch)
        self.assertNotIn('MaizoForceVI704',bridge)

if __name__=="__main__":
    unittest.main()
