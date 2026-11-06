#!/usr/bin/env python3
"""Host source-contract regressions; NEVER substitutes for real Wii tests."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def src(path):return (ROOT/path).read_text(encoding="utf-8")
class FirstReleaseRegression(unittest.TestCase):
    def test_gc_autoboot_contract_never_truncates(self):
        a=src("source/launch_args.c")
        launch=src("source/launch.c")
        self.assertIn("char game_path[255]",a)
        self.assertIn("n>=cap",a)
        self.assertIn("MaizoNinCfgSizeCheck",launch)
        self.assertIn("maizo_gc_relative_path(g->path,n.GamePath",launch)
    def test_stock_wiimc_media_engine_remains(self):
        p=src("tools/patch_upstreams.py")
        self.assertIn("WiiMC browser null device entry guard",p)
        self.assertIn("WiiMC browser empty-parent underflow guard",p)
        self.assertIn("WiiMC empty media filename GetExt guard",p)
        self.assertIn("WiiMC ORIGINAL 3.0 media engine retained",p)
    def test_frontend_geometry_is_not_injected_into_game_profiles(self):
        launch=src("source/launch.c")
        self.assertIn("int x240=(int)c->game_x240-50;",launch)
        self.assertIn("unsigned w480=c->game_w480;",launch)
        self.assertNotIn("c->video_w*c->game_w480",launch)
        self.assertNotIn("c->video_x+(int)c->game_x240",launch)
    def test_nine_resident_icons_and_visible_selection_ring(self):
        ui=src("source/ui.c")
        cover=src("source/covers.c")
        self.assertIn("tab_halo((int)x+22,31+dy)",ui)
        self.assertIn("cover_draw_icon(&icon,x+10,17,24,30)",ui)
        self.assertIn("static Cache cache[27]",cover)
        self.assertIn("const int first=kind==1?0:kind==2?9:18",cover)
        self.assertNotIn("fopen(",ui)
    def test_homebrew_details_show_meta_description_and_per_game_options(self):
        lib=src("source/library.c")
        main=src("source/main.c")
        ui=src("source/ui.c")
        self.assertIn("<short_description>",lib)
        self.assertIn("g->description",ui)
        self.assertIn("game_metadata(g,393)",ui)
        self.assertIn("state.settings_return=PAGE_DETAIL",main)
        self.assertIn("state.page=state.settings_return",main)
    def test_oled_first_run_does_not_overwrite_user_settings(self):
        c=src("source/config.c")
        self.assertIn("c->theme=4;c->layout=2;",c)
        self.assertIn('if(!strcmp(key,"theme"))c->theme=val%6;',c)
if __name__=="__main__":unittest.main()
