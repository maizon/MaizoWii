#!/usr/bin/env python3
"""WiiMC v6.3 source regression: retain stock media and GUI code."""
import ast
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class WiiMCMinimalAdapter(unittest.TestCase):
 def test_wiimc_isolated_patch(self):
  s=(ROOT/"tools/patch_upstreams.py").read_text()
  ast.parse(s)
  a=s.index("def patch_wiimc(")
  b=s.index("def patch_wiisx422(",a)
  section=s[a:b]
  self.assertIn("maizo_hid_tick_attached",section)
  self.assertIn("maizo_hid_down",s[:a])
  self.assertIn("maizo_hid_held",s[:a])
  self.assertNotIn("patch_file(root, \"source/video.cpp\"",section)
  self.assertNotIn("patch_file(root, \"source/menu.cpp\"",section)
  self.assertNotIn("player_deadlock_guards",section)
  self.assertNotIn("VIDEO_SetBlack(FALSE)",section)
  self.assertNotIn("AutobootExit = false",section)
  self.assertNotIn("controlledbygui=1",section)
  self.assertIn("source/input.cpp",section)
  self.assertIn("WiiMC ORIGINAL 3.0 media engine retained",section)
  self.assertIn("maizo_hid_tick_attached();",section)
  self.assertIn("maizo_hid_tick();",section)
if __name__=="__main__":unittest.main()
