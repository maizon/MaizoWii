#!/usr/bin/env python3
"""Static constraints for the Maizo minimal carousel and always-on brand.
Hardware/appearance must still be checked on a physical Wii/CRT.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ui = (ROOT / "source/ui.c").read_text()
main = (ROOT / "source/main.c").read_text()

assert "footer(u);" in ui
assert "if(i->favorite && state.page==PAGE_LIBRARY)" in main
assert "static void brand_overlay(void){" in ui
brand = ui.split("static void brand_overlay(void){", 1)[1].split("\n}", 1)[0]
assert 'label(25,23,"MAIZO",2.0f' in brand
assert 'label(25,23+dy,"MAIZO"' not in ui
topbar = ui.split("static void topbar(", 1)[1].split("static void brand_overlay", 1)[0]
assert 'label(25,23,"MAIZO"' not in topbar

render = ui.split("void ui_draw(", 1)[1].split("void ui_shutdown", 1)[0]
assert render.rfind("brand_overlay();") > render.rfind("footer(u);")
assert render.rfind("brand_overlay();") > render.rfind("topbar(u,l,hid);")
assert render.rfind("brand_overlay();") < render.rfind("maizo_video_present();")
assert "else topbar_phase=0;\n /* LAST overlay pass:" in render

boot = ui.split("void ui_boot_message(", 1)[1].split("int ui_init(", 1)[0]
assert "brand_overlay();" in boot
assert "maizo_video_present();" in boot
print("PASS premium carousel and always-on upper-left MAIZO branding")
