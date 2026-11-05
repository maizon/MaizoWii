#!/usr/bin/env python3
"""Deterministic host regression tests for the gamepad-first Wii UI.
These tests exercise production nav.c, not a Python reimplementation."""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class MenuNavigation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which("cc") or shutil.which("gcc")
        if not cc:
            raise unittest.SkipTest("host C compiler not installed")
        cls.temp=tempfile.TemporaryDirectory()
        so=Path(cls.temp.name)/"nav.so"
        subprocess.run([cc,"-std=c11","-O2","-Wall","-Wextra","-Werror",
                        "-shared","-fPIC","-I",str(ROOT/"include"),
                        str(ROOT/"source/nav.c"),"-o",str(so)],check=True)
        cls.lib=ctypes.CDLL(str(so))
        cls.lib.maizo_nav_move.argtypes=[ctypes.c_int]*5
        cls.lib.maizo_nav_scroll.argtypes=[ctypes.c_int]*5

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def move(self,index,total,columns,horizontal,direction):
        return self.lib.maizo_nav_move(index,total,columns,horizontal,direction)

    def scroll(self,index,offset,total,columns,visible=8):
        return self.lib.maizo_nav_scroll(index,offset,total,columns,visible)

    def test_empty_library_never_divides_by_zero(self):
        for columns in (0,1,4):
            self.assertEqual(self.move(0,0,columns,0,-1),0)
            self.assertEqual(self.scroll(0,20,0,columns),0)

    def test_one_item_both_axes(self):
        for horizontal in (0,1):
            for direction in (-1,1):
                self.assertEqual(self.move(0,1,4,horizontal,direction),0)

    def test_full_grid_preserves_column_and_wraps(self):
        self.assertEqual(self.move(2,12,4,0,1),6)
        self.assertEqual(self.move(2,12,4,0,-1),10)
        self.assertEqual(self.move(6,12,4,1,-1),5)
        self.assertEqual(self.move(7,12,4,1,1),4)

    def test_short_final_row_is_never_out_of_bounds(self):
        for size in range(1,35):
            for index in range(size):
                for horizontal in (0,1):
                    for direction in (-1,1):
                        result=self.move(index,size,4,horizontal,direction)
                        self.assertGreaterEqual(result,0)
                        self.assertLess(result,size)
        self.assertEqual(self.move(3,5,4,0,1),4)

    def test_list_up_down_wrap_without_lateral_motion(self):
        self.assertEqual(self.move(0,7,1,0,-1),6)
        self.assertEqual(self.move(6,7,1,0,1),0)
        self.assertEqual(self.move(3,7,1,1,1),3)

    def test_scroll_remains_grid_aligned(self):
        for total in (1,4,5,9,17,51):
            scroll=0
            for selected in range(total):
                scroll=self.scroll(selected,scroll,total,4)
                self.assertEqual(scroll%4,0)
                self.assertLessEqual(scroll,selected)
                self.assertLess(selected,scroll+8)
        self.assertEqual(self.scroll(11,0,12,4),4)

    def test_list_scroll(self):
        self.assertEqual(self.scroll(10,0,18,1),3)
        self.assertEqual(self.scroll(2,9,18,1),2)

class UiIntegration(unittest.TestCase):
    def read(self,path):
        return (ROOT/path).read_text(encoding="utf-8")

    def test_real_frontend_uses_tested_navigation(self):
        main=self.read("source/main.c")
        self.assertIn('#include "nav.h"',main)
        self.assertIn("maizo_nav_move(state.selected,n,columns,0,-1)",main)
        self.assertIn("maizo_nav_scroll(state.selected,state.scroll,count",main)
        self.assertNotIn("stride%n",main)

    def test_first_boot_opens_carousel_and_usb_mount_still_runs(self):
        main=self.read("source/main.c")
        config=self.read("source/config.c")
        self.assertIn("int storage_ready=mount_storage(1)",main)
        self.assertIn("input_storage_ready(media_ready)",main)
        self.assertIn("state.page=PAGE_HOME;",main)
        self.assertNotIn("storage_ready&&!config_has_settings()",main)
        self.assertIn("config_load(&config)",main)
        self.assertIn("c->layout=2;",config)
        self.assertIn("c->theme=3",config)

    def test_info_returns_to_relevant_context(self):
        main=self.read("source/main.c")
        self.assertIn("state.page=state.info_return",main)
        self.assertIn("state.info_return=PAGE_DETAIL",main)
        self.assertIn("state.info_return=PAGE_SETTINGS",main)

    def test_media_official_and_hid_builds_are_separate(self):
        launch=self.read("source/launch.c")
        ui=self.read("source/ui.c")
        main=self.read("source/main.c")
        self.assertIn("module_variant_available(System sys,const Config*c)",launch)
        self.assertIn("alternate_media=resolve_media_official()!=NULL",launch)
        self.assertIn("alternate_ps1=resolve_ps1_wiisx()!=NULL",launch)
        self.assertIn("module_variant_available(g->system,&u->game_config)",ui)
        self.assertIn("config_game_load(&state.game_config,&config,&library_state.games[idx])",main)
        self.assertIn("WIIMC ORIGINAL",ui)
        self.assertIn("WIIMC HID",ui)

    def test_ui_is_cpu_only_and_logo_is_exclusive_top_layer(self):
        ui=self.read("source/ui.c")
        main=self.read("source/main.c")
        self.assertNotIn("GX_",ui)
        self.assertNotIn("fopen(",ui)
        self.assertNotIn("USB OK",ui)
        self.assertNotIn("SD OK",ui)
        self.assertIn('label(25,23,"MAIZO",2.0f,ink);',ui)
        self.assertIn("topbar(u,l,hid);",ui)
        self.assertIn("state.storage_usb=storage_usb",main)
        self.assertIn("state.storage_sd=storage_sd",main)
        self.assertIn("if(u->page!=PAGE_HOME && u->page!=PAGE_LIBRARY)footer(u);",ui)

if __name__=="__main__":
    unittest.main()
