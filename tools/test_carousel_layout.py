#!/usr/bin/env python3
"""Execute actual carousel geometry C on host for focus-independent stability."""
import ctypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class Slot(ctypes.Structure):
    _fields_=[("x",ctypes.c_int),("y",ctypes.c_int),("w",ctypes.c_int),("h",ctypes.c_int)]

class CarouselGeometryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which("cc")
        if cc is None: raise unittest.SkipTest("Host compiler unavailable")
        cls.tmp=tempfile.TemporaryDirectory()
        lib=Path(cls.tmp.name)/"layout.so"
        subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
                        "-I",str(ROOT/"include"),str(ROOT/"source/carousel_layout.c"),
                        "-o",str(lib)],check=True,capture_output=True,text=True)
        cls.slot=ctypes.CDLL(str(lib)).maizo_card_slot
        cls.slot.argtypes=[ctypes.c_int,ctypes.POINTER(Slot)]
        cls.slot.restype=ctypes.c_int
    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()
    def test_10000_selections_never_shift_or_jitter_card_slots(self):
        baseline=[]
        for relative in range(-2,3):
            cell=Slot()
            self.assertEqual(self.slot(relative,ctypes.byref(cell)),1)
            baseline.append((cell.x,cell.y,cell.w,cell.h))
        for _ in range(2000):
            for relative,reference in zip(range(-2,3),baseline):
                cell=Slot()
                self.assertEqual(self.slot(relative,ctypes.byref(cell)),1)
                self.assertEqual((cell.x,cell.y,cell.w,cell.h),reference)
        self.assertEqual(baseline[2],(240,125,160,224))
    def test_aspect_and_screen_safety(self):
        for relative in range(-2,3):
            cell=Slot()
            self.slot(relative,ctypes.byref(cell))
            self.assertGreater(cell.x,0)
            self.assertGreater(cell.y,100)
            self.assertLessEqual(cell.x+cell.w,640)
            self.assertLessEqual(cell.y+cell.h,480)
            self.assertLess(abs(cell.w/cell.h-5/7),0.007)
        self.assertEqual(self.slot(-3,ctypes.byref(Slot())),0)
        self.assertEqual(self.slot(3,ctypes.byref(Slot())),0)
        self.assertEqual(self.slot(0,None),0)
    def test_no_global_translation_backsliding(self):
        ui=(ROOT/"source/ui.c").read_text()
        self.assertIn("maizo_card_slot(delta,&slot)",ui)
        self.assertNotIn("x+=shift",ui)
        self.assertNotIn("remaining*remaining",ui)

if __name__=="__main__":
    unittest.main()
