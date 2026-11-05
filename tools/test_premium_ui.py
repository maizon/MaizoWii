#!/usr/bin/env python3
"""Static guardrails for the Maizo minimal carousel; not a hardware test."""
from pathlib import Path
import re

ui = Path("source/ui.c").read_text()
config = Path("source/config.c").read_text()
main = Path("source/main.c").read_text()
icons = Path("tools/prepare_system_icons.py").read_text()
assert "c->layout=2;" in config, "Carousel must be the factory default"
assert "state.page=PAGE_HOME;" in main
assert "static const int order[5]={-2,2,-1,1,0}" in ui
assert "if(u->page!=PAGE_HOME && u->page!=PAGE_LIBRARY)footer(u);" in ui
assert ui.rfind("topbar(u,l,hid);") > ui.rfind("footer(u);"), "Logo not on top layer"
assert not re.search(r"i->favorite", main), "Favorites actions still exposed"
for key in ("IWII", "IGC", "IPS1", "ISNES", "INES", "IGBA", "IGB", "IMEDIA"):
    assert key in ui and key in icons, "Missing icon cache key " + key
assert "ImageOps.contain" in Path("tools/build_cover_cache.py").read_text()
print("PASS premium carousel, no favorites, aspect-preserving imagery")
