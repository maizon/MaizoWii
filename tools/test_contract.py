#!/usr/bin/env python3
"""MAIZO contract invariants: fast, dependency-free static CI checks."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text(encoding="utf-8")

header = read("include/app.h")
library = read("source/library.c")
launch = read("source/launch.c")
launch_args = read("source/launch_args.c")
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
          "Core=%u", "Audio=%u", "SkipFrames=%u", "usb1:/%s"):
    assert k in (launch + launch_args), k
assert "maizo_build_args(&spec,&command" in launch
assert "args->argc=command.argc" in launch
assert "APPEND_TEXT" not in launch
assert "MAIZO_BOOT_ADDR" in launch and "maizo_bootstub" in launch
assert "static u32 install_dol" not in launch
assert "SYS_ResetSystem(SYS_SHUTDOWN" in launch
assert "bootstub_bytes.o" in read("Makefile")
assert "copy_bytes" in read("bootstub/stub.c")
assert "0x93000000" in read("bootstub/link.ld")
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

# Nintendont-style MultiIn parsing is shared by all compiled HID adapters.
hid = read("integrations/maizo_hid.inc")
assert "mh_profile.multiin==4" in hid
assert "mh_report[0]" in hid and "mh_live[chan]" in hid
assert "maizo_hid_connected(int chan)" in hid
forwarder = read("forwarder/main.c")
assert "sd:/apps/maizo/boot.dol" in forwarder
assert "usb:/apps/maizo/boot.dol" in forwarder
