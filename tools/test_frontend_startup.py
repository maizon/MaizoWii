#!/usr/bin/env python3
"""Source-level regression checks for Wii frontend startup (not hardware tests)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

def source(p):
    return (ROOT / p).read_text(encoding="utf-8")

class WiiFrontendBootSafety(unittest.TestCase):
    def test_video_before_storage_or_usb(self):
        main = source("source/main.c")
        boot = main[main.index("int main(int argc,char**argv){"):]
        self.assertLess(boot.index("ui_init()"), boot.index("fatInitDefault()"))
        self.assertLess(boot.index('ui_boot_message("VIDEO GX OK")'),
                        boot.index("fatInitDefault()"))
        self.assertLess(boot.index("ui_draw(&state"), boot.index("input_poll(&in)"))

    def test_xfb_never_unblanked_uninitialized(self):
        ui = source("source/ui.c")
        self.assertIn("GX_CopyDisp(framebuffer[which],GX_TRUE)",ui)
        self.assertIn("GX_DrawDone(); /* ensure the new XFB",ui)
        self.assertLess(ui.index("GX_CopyDisp(framebuffer[which],GX_TRUE)"),
                        ui.index("VIDEO_SetBlack(FALSE)"))
        init = ui[ui.index("int ui_init(void){"):ui.index("void ui_boot_message(")]
        self.assertIn("VIDEO_SetBlack(TRUE)",init)
        self.assertNotIn("VIDEO_SetBlack(FALSE)",init)
        self.assertIn("GX_LoadPosMtxImm",init)
        self.assertIn("GX_SetChanCtrl",init)

    def test_no_disk_probing_from_draw_loop(self):
        launch=source("source/launch.c")
        ui=source("source/ui.c")
        self.assertIn("void module_refresh_available(void)",launch)
        self.assertIn("modules_present[sys]",launch)
        self.assertNotIn("fopen(",ui)

    def test_usb_hid_has_delayed_safe_mode(self):
        inp=source("source/input.c")
        self.assertIn("input_frames>90",inp)
        self.assertIn("disable-hid.flag",inp)
        self.assertIn("if(hid_started)maizo_hid_shutdown()",inp)

    def test_cover_lifetime_and_tevs(self):
        covers=source("source/covers.c")
        self.assertIn("GX_DrawDone();free(c->pixels)",covers)
        self.assertIn("GX_InvalidateTexAll()",covers)
        self.assertIn("GX_SetTevOrder",covers)
        self.assertIn("c->system==(int)g->system",covers)

if __name__ == "__main__":
    unittest.main()
