#!/usr/bin/env python3
"""Adapt the pinned GPL-2.0 YAWM ModMii Edition source for MAIZO USB HID.

The precompiled official YAWM is NOT patched. Only a successful DOL build
from this source may be marked "hid_adapted". Physical NAND/HID tests remain
mandatory before release. The WAD install confirmation is unchanged.
"""
from pathlib import Path
import argparse
import shutil

ROOT = Path(__file__).resolve().parents[1]


def once(source: str, old: str, new: str, name: str) -> str:
    count = source.count(old)
    if count != 1:
        raise ValueError(f"{name}: expected 1 occurrence, got {count}: {old!r}")
    return source.replace(old, new, 1)


def apply(source: Path) -> None:
    if not source.is_dir() or not (source / "source/menu.c").is_file():
        raise ValueError("YAWM source checkout is missing")
    dest = source / "source"
    for name in ("maizo_hid.inc", "maizo_report.h", "maizo_multi.h", "yawm_cache_compat.c"):
        shutil.copyfile(ROOT / "integrations" / name, dest / name)

    shutil.copyfile(ROOT / "integrations" / "yawm_sda_anchors.c",
                    dest / "maizo_sda_anchors.c")
    menu = dest / "menu.c"
    text = menu.read_text()
    text = once(text, '#include "wpad.h"',
                '#include "wpad.h"\n#include "maizo_hid.inc"',
                "YAWM shared HID adapter")
    text = once(text,
                '\t\tbuttonsGC = Pad_GetButtons();',
                '\t\tbuttonsGC = Pad_GetButtons();\n'
                '\t\t/* Physical GC pads and class USB HID gamepads share the\n'
                '\t\t * existing, explicitly confirmed WAD menu actions. */\n'
                '\t\tmaizo_hid_tick();\n'
                '\t\tfor(int port=0; port<4; port++)\n'
                '\t\t\tbuttonsGC |= maizo_hid_down(port);',
                "YAWM HID navigation")
    text = once(text,
                '\t\tFatUnmount();\n\t\tWpad_Disconnect();',
                '\t\tFatUnmount();\n\t\tmaizo_hid_shutdown();\n'
                '\t\tWpad_Disconnect();',
                "YAWM IOS reload shutdown")
    if text.count("Wpad_Init();") != 2:
        raise ValueError("Unexpected YAWM IOS reload count")
    text = text.replace("Wpad_Init();",
                        "Wpad_Init();\n\t\t\tmaizo_hid_resume();")
    appboot = source / "source/appboot.c"
    if appboot.is_file():
        app_text = appboot.read_text()
        app_text = once(app_text, "#include <ogc/lwp_threads.h>",
                        "#include <ogc/lwp.h>",
                        "YAWM current libogc LWP headers")
        appboot.write_text(app_text)
    # Upstream defines its own Korean-key check, while current libogc
    # exports a different ES_CheckHasKoreanKey. Keep the exact upstream
    # implementation but rename the local symbol to avoid link/type clash.
    for name in ("sys.c", "sys.h"):
        local = dest / name
        if local.is_file():
            contents = local.read_text()
            if "ES_CheckHasKoreanKey" not in contents:
                raise ValueError("YAWM missing Korean key helper")
            local.write_text(contents.replace(
                "ES_CheckHasKoreanKey", "YAWM_CheckHasKoreanKey"))
    old_sys = dest / "sys.c"
    if old_sys.is_file():
        sys_src = old_sys.read_text()
        sys_src = once(
            sys_src, "(u8 *)ios_tmd, tmd_size);",
            "ios_tmd, tmd_size);",
            "YAWM libogc modern typed ES TMD view")
        old_sys.write_text(sys_src)
    title_file = dest / "title.c"
    if title_file.is_file():
        title_src = title_file.read_text()
        title_src = once(title_src, "ES_GetTMDView(tid, (u8*) view, view_sz);",
                         "ES_GetTMDView(tid, view, view_sz);",
                         "YAWM typed ES title view")
        # The upstream GetSize routine frees a nonexistent 'view' variable.
        # Free the actual signed TMD returned by Title_GetTMD.
        title_src = once(title_src, "free(view);\n\n\treturn 0;",
                         "free(p_tmd);\n\n\treturn 0;",
                         "YAWM title size incorrect free")
        title_file.write_text(title_src)
    # Modern libogc/newlib expects r13 small-data references in a bounded
    # +/-32KiB window. Avoid moving YAWM's own globals into the small-data
    # section: otherwise the libogc CRT/keyboard references overflow SDA21
    # relocations at link time. Keep linker flags untouched.
    makefile = source / "Makefile"
    if makefile.is_file():
        make_src = makefile.read_text()
        make_src = once(make_src, "CFLAGS\t= -Os -Wall $(MACHDEP) $(INCLUDE)",
                        "CFLAGS\t= -Os -Wall -G0 $(MACHDEP) $(INCLUDE)",
                        "YAWM libogc small-data relocation bounds")
        make_src = once(
            make_src, "-Wl,-Map,$(notdir $@).map",
            "-Wl,-Map,$(notdir $@).map "
            "-Wl,--defsym,_SDA_BASE_=maizo_sda_anchor+0x7e00 "
            "-Wl,--defsym,_SDA2_BASE_=maizo_sda2_anchor+0x7e00",
            "YAWM correct PPC SDA base for libogc")
        makefile.write_text(make_src)
    menu.write_text(text)
    # The pre-devkitPro libogc bootstrap called a private exception cleanup
    # entrypoint removed from current public libogc. The loader already
    # shuts down the console and disables interrupts before jumping.
    loader = source / "source/boot/source/main.c"
    if loader.is_file():
        old = loader.read_text()
        updated = once(old, "\t__exception_closeall();",
                       "    /* legacy private exception cleanup absent from current libogc */",
                       "YAWM bootloader legacy exception API")
        loader.write_text(updated)

    print("PATCH YAWM menu.c: HID class detection, Nintendont profiles, "
          "IOS reload and confirmation-preserving navigation")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    apply(args.source)


if __name__ == "__main__":
    main()
