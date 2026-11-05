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
        self.assertLess(boot.index("ui_init()"), boot.index("mount_storage()"))
        self.assertLess(boot.index('ui_boot_message("VIDEO VI OK")'),
                        boot.index("mount_storage()"))
        self.assertLess(boot.index("ui_draw(&state"), boot.index("input_poll(&in)"))
        self.assertIn('fatMountSimple("usb",&__io_usbstorage)',main)
        self.assertIn('input_storage_ready(media_ready)',main)
        self.assertIn('fatMountSimple("sd",&__io_wiisd)',main)

    def test_software_framebuffer_rendering(self):
        video=source("source/video_sw.c")
        self.assertIn("MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode))",video)
        self.assertIn("VIDEO_SetNextFramebuffer(buffers[back])",video)
        self.assertIn("VIDEO_SetBlack(FALSE)",video)
        self.assertIn("VIDEO_GetPreferredMode(NULL)",video)
        self.assertIn("yuyv_pair(",video)
        self.assertIn("set_pair(dest,pair,x0,x1",video)
        self.assertIn("maizo_video_cover(",video)
        self.assertNotIn("GX_",video)
        self.assertNotIn("GX_",source("source/ui.c"))
        self.assertNotIn("GX_",source("source/covers.c"))
        self.assertLess(video.index("VIDEO_SetNextFramebuffer(buffers[back])"),
                        video.index("VIDEO_SetBlack(FALSE)"))

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

    def test_cover_cache_decodes_gx_tiles_without_gpu(self):
        covers=source("source/covers.c")
        video=source("source/video_sw.c")
        self.assertIn("cache[j].system==(int)g->system",covers)
        self.assertIn("maizo_video_cover((const unsigned char*)c->pixels",covers)
        self.assertIn("((ty/4)*32+tx/4)*64",video)
        self.assertIn("source[base+32]",video)
        self.assertIn("source[base+33]",video)
        self.assertNotIn("GX_InitTexObj",covers)

    def test_cover_gx_tile_offsets_never_exceed_image(self):
        width,height=128,160
        data_size=width*height*4
        for y in range(height):
            for x in range(width):
                base=((y//4)*32+x//4)*64+(((y&3)*4+(x&3))*2)
                self.assertLess(base+33,data_size)

if __name__ == "__main__":
    unittest.main()
