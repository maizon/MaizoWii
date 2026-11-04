#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
get=lambda p:(r/p).read_text()
h=get("include/app.h");lib=get("source/library.c")
for x in ("SYSTEM_GC","SYSTEM_PS1","SYSTEM_MEDIA","library_step","module_launch","input_poll","ui_draw"):assert x in h,x
for x in (".iso",".gcm",".cue",".chd",".pbp",".avi",".mp4",".mkv","usb:/games","sd:/ps1"):assert x in lib,x
for x in ("NIN_CFG_MAGIC","NIN_CFG_AUTO_BOOT","nincfg.bin","boot.dol","Core="):assert x in get("source/launch.c"),x
assert "USB_ReadIntrMsgAsync" in get("source/input.c")
assert "APPEND_TEXT(media_path)" in get("source/launch.c")
assert "usb1:/" in get("source/launch.c")
assert "sd1:/" in get("source/launch.c")
assert "GX_Begin" in get("source/ui.c")
assert "GX_InitTexObj" in get("source/covers.c")
assert "MZG1" in get("tools/build_cover_cache.py")
assert "VIDEO_WaitVSync" in get("source/main.c")
print("PASS MAIZO frontend integration and static contracts")
