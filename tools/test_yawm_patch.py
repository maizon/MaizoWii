#!/usr/bin/env python3
"""Contract tests: YAWM HID integration must not bypass WAD confirmations."""
import tempfile
from pathlib import Path
import unittest
from patch_yawm import apply

class InstallerHidPatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "source").mkdir()
        self.menu = self.root / "source/menu.c"
        self.original = (
            '#include "wpad.h"\n'
            'void ios_change(void) {\n'
            '\t\tFatUnmount();\n\t\tWpad_Disconnect();\n'
            '\t\tWpad_Init();\n'
            '\t\tWpad_Init();\n'
            '}\n'
            'void WaitButtons(void) {\n'
            '\t\tbuttonsGC = Pad_GetButtons();\n'
            '\t\tVIDEO_WaitVSync();\n'
            '}\n'
            'int install(void) {\n'
            '    /* Existing mandatory confirmation cannot be removed. */\n'
            '    if (buttons & WPAD_BUTTON_A) confirm_install();\n'
            '    return 0;\n}\n'
        )
        self.menu.write_text(self.original)
        (self.root / "Makefile").write_text(
            "CFLAGS\t= -Os -Wall $(MACHDEP) $(INCLUDE)\n"
            "LDFLAGS\t= $(MACHDEP) -Wl,-Map,$(notdir $@).map\n")

    def test_hid_input_and_confirmation_preserved(self):
        apply(self.root)
        patched = self.menu.read_text()
        self.assertIn('maizo_hid.inc', patched)
        self.assertIn('maizo_hid_tick()', patched)
        self.assertIn('maizo_hid_down(port)', patched)
        self.assertIn('maizo_hid_shutdown()', patched)
        self.assertEqual(patched.count('maizo_hid_resume()'), 2)
        self.assertIn('if (buttons & WPAD_BUTTON_A) confirm_install();', patched)
        self.assertEqual((self.root/'source/maizo_report.h').is_file(), True)
        self.assertEqual((self.root/'source/maizo_hid.inc').is_file(), True)
        self.assertTrue((self.root/'source/maizo_multi.h').is_file())
        self.assertTrue((self.root/"source/yawm_cache_compat.c").is_file())
        self.assertIn("void DCInvalidateRange(", (self.root/"source/yawm_cache_compat.c").read_text())
        self.assertIn("-G0", (self.root/"Makefile").read_text())
        self.assertIn("--defsym,_SDA_BASE_=maizo_sda_anchor+0x7e00",
                      (self.root/"Makefile").read_text())
        self.assertIn("--defsym,_SDA2_BASE_=maizo_sda2_anchor+0x7e00",
                      (self.root/"Makefile").read_text())
        self.assertIn("maizo_sda2_anchor", (self.root/"source/maizo_sda_anchors.c").read_text())

    def test_legacy_devkitpro_patches_keep_nand_flow(self):
        (self.root/"source/boot/source").mkdir(parents=True)
        (self.root/"source/boot/source/main.c").write_text(
            "void boot(){\n\t__exception_closeall();\n}\n")
        (self.root/"source/appboot.c").write_text(
            '#include <ogc/lwp_threads.h>\n')
        (self.root/"source/sys.h").write_text(
            'bool ES_CheckHasKoreanKey(void);\n')
        (self.root/"source/sys.c").write_text(
            'bool ES_CheckHasKoreanKey(void) {\n'
            'return ES_GetTMDView(0, (u8 *)ios_tmd, tmd_size);\n}\n')
        (self.root/"source/title.c").write_text(
            'ret = ES_GetTMDView(tid, (u8*) view, view_sz);\n'
            'free(view);\n\n\treturn 0;\n')
        apply(self.root)
        self.assertNotIn("__exception_closeall();",
                         (self.root/"source/boot/source/main.c").read_text())
        self.assertIn("<ogc/lwp.h>", (self.root/"source/appboot.c").read_text())
        sys_source=(self.root/"source/sys.c").read_text()
        self.assertIn("YAWM_CheckHasKoreanKey",sys_source)
        self.assertIn("ES_GetTMDView(0, ios_tmd, tmd_size)",sys_source)
        title=(self.root/"source/title.c").read_text()
        self.assertIn("ES_GetTMDView(tid, view, view_sz)",title)
        self.assertIn("free(p_tmd);",title)
        self.assertNotIn("free(view);",title)

    def test_missing_signature_is_error_not_fake_patch(self):
        self.menu.write_text(self.original.replace('\t\tbuttonsGC = Pad_GetButtons();',
                                                   '\t\tbuttonsGC = 0;'))
        with self.assertRaises(ValueError):
            apply(self.root)

    def test_twice_fails_closed(self):
        apply(self.root)
        with self.assertRaises(ValueError):
            apply(self.root)

if __name__ == '__main__':
    unittest.main()
