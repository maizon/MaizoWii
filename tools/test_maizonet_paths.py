#!/usr/bin/env python3
"""Regression: fresh experimental Nintendont must not be hidden by old bundle."""
import pathlib
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class TestMaizoNetPaths(unittest.TestCase):
    def test_experimental_apps_path_first(self):
        text=(ROOT/"source/launch.c").read_text()
        a=text.index("static const char *maizonet_nintendont_path")
        region=text[a:text.index("static const char*ps1_wiisx_paths",a)]
        fresh=region.index('usb:/apps/nintendont-maizonet/boot.dol')
        legacy=region.index('usb:/maizo/modules/nintendont-maizonet/boot.dol')
        self.assertLess(fresh,legacy)
if __name__=="__main__":
    unittest.main()
