#!/usr/bin/env python3
"""MAIZO contract invariants: fast, dependency-free static CI checks."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text(encoding="utf-8")

header = read("include/app.h")
library = read("source/library.c")
launch = read("source/launch.c")
input_src = read("source/input.c")
ui = read("source/ui.c")
settings = read("source/settings.c")
config = read("source/config.c")
expected = (
    "SYSTEM_WII", "SYSTEM_GC", "SYSTEM_PS1", "SYSTEM_SNES",
    "SYSTEM_NES", "SYSTEM_GBA", "SYSTEM_GB", "SYSTEM_MEDIA",
)
for name in expected:
    assert name in header and name in library and name in launch, name
assert "SYSTEM_COUNT" in header
assert "paths[SYSTEM_COUNT][4]" in launch
for module in ("usbloader_gx", "Nintendont", "WiiSXRX",
               "snes9xgx", "fceugx", "vbagx", "WiiMC"):
    assert module in launch, module
for extension in (".wbfs", ".iso", ".gcm", ".cue", ".bin",
                  ".sfc", ".smc", ".nes", ".gba", ".gbc",
                  ".mp4", ".mkv", ".flac"):
    assert extension in library, extension
for k in ("nincfg.bin", "NIN_CFG_AUTO_BOOT", "NIN_CFG_MC_MULTI",
          "NIN_CFG_NATIVE_SI", "-bootios=%u", "-usbport=%u",
          "Core=%u", "Audio=%u", "SkipFrames=%u", "APPEND_TEXT(media_path)"):
    assert k in launch, k
assert "SYS_ResetSystem(SYS_SHUTDOWN" not in launch
for k in ("USB_ReadIntrMsgAsync", "profile_read", "decode_profile",
          "USB_GetDeviceList", "PAD_StickX", "broken"):
    assert k in input_src, k
for k in ("settings_count", "settings_value", "settings_change",
          "gc_video", "wii_ios", "media_volume"):
    assert k in settings + config, k
for k in ("GX_Begin", "search_ui", "system_title", "cover_draw",
          "module_available", "GRADE"):
    assert k in (ui + settings), k
assert "GX_InitTexObj" in read("source/covers.c")
assert "MZG1" in read("tools/build_cover_cache.py")
assert "VIDEO_WaitVSync" in read("source/main.c")
assert "0x40" in read("profiles/0079_0006.ini")
print("PASS: 8 systems, module registry, UI, profiles, launch and cover contracts")
