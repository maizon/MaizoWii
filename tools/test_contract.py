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
for k in ("maizo_hid_tick", "maizo_hid_held", "maizo_hid_shutdown",
          "PAD_StickX", "WPAD_ButtonsHeld", "HidDiagnostics"):
    assert k in input_src + header, k
for k in ("settings_count", "settings_value", "settings_change",
          "gc_video", "wii_ios", "media_volume"):
    assert k in settings + config, k
for k in ("maizo_video_rect", "search_ui", "system_title", "cover_draw",
          "module_available", "GRADE"):
    assert k in (ui + settings), k
assert "maizo_video_cover" in read("source/covers.c")
assert "MZG1" in read("tools/build_cover_cache.py")
assert "VIDEO_WaitVSync" in read("source/main.c")
assert "VIDEO_WaitVSync" in read("source/video_sw.c")
assert "SYS_AllocateFramebuffer" in read("source/video_sw.c")
assert "0x40" in read("profiles/0079_0006.ini")
print("PASS: 8 systems, module registry, UI, profiles, launch and cover contracts")

# Nintendont-style MultiIn parsing is shared by all compiled HID adapters.
hid = read("integrations/maizo_hid.inc")
assert "mh_profile.multiin==4" in hid
assert "maizo_route_reports(" in hid and "mh_decode(mh_report+p.offset" in hid
assert "mh_report[0]" in hid and "mh_live[chan]" in hid
assert "maizo_hid_connected(int chan)" in hid
forwarder = read("forwarder/main.c")
assert "sd:/apps/maizo/boot.dol" in forwarder
assert "usb:/apps/maizo/boot.dol" in forwarder

# Shared HID driver really is linked into the frontend and every patchable
# module rather than duplicating a VID/PID-only parser.
assert '#include "../integrations/maizo_hid.inc"' in input_src
for symbol in ("mzr_parse", "mzr_decode", "USB_GetGenericDescriptor",
               "USB_ReadIntrMsgAsync", "mh_profile.multiin"):
    assert symbol in hid, symbol
assert "maizo_multi.h" in read("tools/patch_upstreams.py")
assert "maizo_hid_down(port)" in read("tools/patch_yawm.py")
assert "installer-hid:" in read(".github/workflows/build.yml")

# Per-game configurations are stored beside the game medium, never via
# the global settings.ini writer, and are merged only at launch.
config_source=read("source/config.c")
main_source=read("source/main.c")
ui_source=read("source/ui.c")
for marker in ("config_game_load(", "config_game_save(",
               "maizo_game_profile_path(", "game-profiles"):
    assert marker in config_source, marker
for marker in ("config_game_load(&effective", "config_game_save(",
               "state.settings_for_game"):
    assert marker in main_source, marker
assert "START: AJUSTES DESTE JOGO" in ui_source
assert "game_profile.h" in config_source

# Controller diagnostics are reached and exited solely by gamepad events.
app_header=read("include/app.h")
for mark in ("PAGE_HID", "last_report_id", "connected_ports",
             "stick_x[4]", "profile_loaded"):
    assert mark in app_header, mark
for mark in ("state.page=PAGE_HID", "if(state.page==PAGE_HID)",
             "state.page=PAGE_SETTINGS"):
    assert mark in main_source, mark
for mark in ("DIAGNOSTICO USB HID", "BOT: %04X",
             "ROTEAMENTO MULTIIN", "B OU START"):
    assert mark in ui_source, mark

# NeoStation-inspired optional graphite palette; no CRT mode switching.
theme_settings=read("source/settings.c")
assert '"GRAFITE"' in theme_settings and "lim=4" in theme_settings
assert "c->theme=val%4" in config_source
assert "graphite=c->theme==3" in ui_source

# Release is blocked if YAWM fails to compile, rather than showing a
# green ALL-IN-ONE artifact that silently contains the unmodified manager.
workflow=read(".github/workflows/build.yml")
assert "Require HID YAWM for ALL-IN-ONE release" in workflow
assert 'manager.get("hid_adapted") is True' in workflow

# Wireless HID receivers may expose a vendor-class interface even when
# they ship a Nintendont controller.ini profile. Only scan those devices
# when the user supplied a matching profile, never generic vendor devices.
for marker in ("for(int pass=0;pass<2;pass++)",
               "USB_GetDeviceList(list,32,filter,&count)",
               "if(pass&&!known)continue",
               "USB_CLASS_VENDOR_SPEC", "e->wMaxPacketSize&0x07ffu"):
    assert marker in hid, marker
