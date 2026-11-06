#!/usr/bin/env python3
"""Premium 1.0 work-in-progress contract tests; never substitute for Wii tests."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


class PremiumUpdateContracts(unittest.TestCase):
    def test_boot_splash_is_black_and_centered(self):
        ui = source("source/ui.c")
        section = ui[ui.index("void ui_boot_message("):ui.index("int ui_init(")]
        self.assertIn("quad(0,0,640,480,0xff000000u,0xff000000u)", section)
        self.assertIn('label(280,223,"MAIZO",2.0f', section)
        self.assertIn("clipped(px,447,stage", section)
        self.assertNotIn("INICIANDO WII HUB", section)
        main = source("source/main.c")
        for stage in ("MONTANDO DISPOSITIVOS...", "CARREGANDO MODULOS...",
                      "VERIFICANDO LISTA DE JOGOS...", "DETECTANDO CONTROLES..."):
            self.assertIn(stage, main)

    def test_aspect_safety_and_legacy_covers(self):
        builder = source("tools/build_cover_cache.py")
        self.assertIn("W,H=128,176", builder)
        self.assertIn("ImageOps.contain", builder)
        self.assertIn('rglob("*.png")', builder)
        ui = source("source/ui.c")
        self.assertIn("maizo_card_slot(delta,&slot)", ui)
        geometry = source("source/carousel_layout.c")
        self.assertIn("{240,127,160,220}", geometry)
        self.assertIn("{116,169,112,154}", geometry)
        self.assertNotIn("x+=shift",ui)
        covers = source("source/covers.c")
        self.assertIn("(hdr[7]!=160&&hdr[7]!=176)", covers)
        self.assertIn("w/wanted", covers)
        video = source("source/video_sw.c")
        self.assertIn("tex_h!=160&&tex_h!=176", video)
        self.assertIn("int ty=(py-y0)*(int)tex_h/", video)

    def test_system_scoped_covers_and_both_devices(self):
        covers = source("source/covers.c")
        for system in ('"wii"', '"gc"', '"ps1"', '"snes"', '"nes"',
                       '"gba"', '"gb"', '"media"'):
            self.assertIn(system, covers)
        self.assertIn('"/maizo/covers/"', covers.replace('%s:/maizo/covers/', '"/maizo/covers/"'))
        self.assertIn('!strncmp(g->path,"sd:/",4)', covers)
        for system in ('wii', 'gc', 'ps1', 'snes', 'nes', 'gba', 'gb'):
            library = source("source/library.c")
            self.assertIn("usb:/", library)
            self.assertIn("sd:/", library)
        self.assertIn('out / folder / (key + ".png")',
                      source("tools/cover_sync.py"))

    def test_direct_play_and_offline_error(self):
        main = source("source/main.c")
        self.assertIn("static void play_game(", main)
        self.assertIn("if(i->favorite && state.page==PAGE_LIBRARY)", main)
        self.assertIn("if(idx>=0)play_game(idx,PAGE_LIBRARY)", main)
        self.assertIn("JOGO INDISPONIVEL.", main)
        self.assertIn("int old_usb=storage_usb,old_sd=storage_sd", main)

    def test_cover_fat_io_is_off_the_render_path(self):
        covers = source("source/covers.c")
        main = source("source/main.c")
        self.assertIn("Cache*c=lookup(g,0)", covers)
        self.assertIn("(void)lookup(g,1)", covers)
        self.assertIn("void cover_reset(void)", covers)
        self.assertIn("prefetch_covers();", main)
        self.assertLess(main.index("ui_draw(&state,&library_state,&config,in.hid_present);"),
                        main.index("  else prefetch_covers();"))
        self.assertIn('ui_boot_message("CARREGANDO CAPAS...")', main)

    def test_vi_software_renderer_avoids_repeated_slow_divisions(self):
        video = source("source/video_sw.c")
        self.assertIn("const int solid=a==b", video)
        self.assertIn("unsigned char tex_x[720]",video)
        self.assertIn("int tx=tex_x[px]",video)

    def test_six_themes_and_dynamic_palette_no_fat_in_draw(self):
        self.assertIn("c->theme=val%6", source("source/config.c"))
        self.assertIn("OLED", source("source/settings.c"))
        ui = source("source/ui.c")
        self.assertIn("cover_tint(g)", ui)
        self.assertIn("previous=0xff000000u|channels", ui)
        self.assertNotIn("fopen(", ui)


if __name__ == "__main__":
    unittest.main()
