#!/usr/bin/env python3
"""Apply explicit, source-level HID patches to pinned standalone Wii upstreams.

This is NOT binary patching. The module must be rebuilt with its own devkitPro
dependencies and tested on hardware. No claim about unsupported XInput devices.
Usage: python3 tools/patch_upstreams.py WiiMC /tmp/WiiMC-SSLC
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil

SOURCE = Path(__file__).resolve().parents[1] / "integrations/maizo_hid.inc"

def replace_once(src: str, old: str, new: str, where: str):
    count = src.count(old)
    if count != 1:
        raise ValueError(f"{where}: expected 1 occurrence of {old!r}, got {count}")
    return src.replace(old, new, 1)

def patch_file(root: Path, rel: str, fun):
    path = root / rel
    original_bytes = path.read_bytes()
    eol = "\r\n" if b"\r\n" in original_bytes else "\n"
    original = original_bytes.decode("utf-8").replace("\r\n", "\n")
    updated = fun(original)
    if original == updated:
        raise ValueError("Patch produced no changes: " + rel)
    path.write_bytes(updated.replace("\n", eol).encode("utf-8"))
    print("PATCH", rel)

def install_inc(root: Path, folder: str):
    dest = root / folder / "maizo_hid.inc"
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(SOURCE, dest)

def fix_pad_update(text: str, key: str, is_wiimc: bool):
    text = replace_once(text, key, key + '\n\tmaizo_hid_tick();', "UpdatePads")
    lines = [
        ("btns_d", "PAD_ButtonsDown", "maizo_hid_down"),
        ("btns_h", "PAD_ButtonsHeld", "maizo_hid_held"),
    ]
    if not is_wiimc:
        lines.append(("btns_u", "PAD_ButtonsUp", "maizo_hid_up"))
    for prop, native, shim in lines:
        text = replace_once(
            text, f"userInput[i].pad.{prop} = {native}(i);",
            f"userInput[i].pad.{prop} = {native}(i) | {shim}(i);",
            "pad." + prop)
    for prop, native, shim in (
        ("stickX", "PAD_StickX", "maizo_hid_stickX"),
        ("stickY", "PAD_StickY", "maizo_hid_stickY"),
    ):
        text = replace_once(text, f"userInput[i].pad.{prop} = {native}(i);",
                            f"userInput[i].pad.{prop} = {shim}(i, {native}(i));", prop)
    return text

def patch_wiimc(root: Path):
    install_inc(root, "source")
    def input_patch(s):
        s = replace_once(s,'#include "input.h"', '#include "input.h"\n#include "maizo_hid.inc"', "WiiMC")
        s = fix_pad_update(s, "PAD_ScanPads();", True)
        s = replace_once(s, "u16 down = PAD_ButtonsDown(cur_chan);",
                         "u16 down = PAD_ButtonsDown(cur_chan) | maizo_hid_down(cur_chan);", "MPlayerInput")
        s = replace_once(s, "u16 held = PAD_ButtonsHeld(cur_chan);",
                         "u16 held = PAD_ButtonsHeld(cur_chan) | maizo_hid_held(cur_chan);", "MPlayerInput")
        return s
    patch_file(root, "source/input.cpp", input_patch)
    patch_file(root, "source/wiimc.cpp",
               lambda s: replace_once(s,"sprintf(loadedFile, argv[1]);",
                  'snprintf(loadedFile, sizeof(loadedFile), "%s", argv[1]);', "WiiMC argv format"))

    def mplayer_patch(t):
        t=replace_once(t, '#include "stream/stream.h"',
          '#include "stream/stream.h"\n#ifndef MAXPATHLEN\n#define MAXPATHLEN 1024\n#endif\n'
          'extern int stream_error(stream_t *);', "WiiMC stream/MPlayer legacy compatibility")
        return t
    patch_file(root, "source/mplayer/mplayer.c", mplayer_patch)
    # Modern iconv expects mutable char** and the old MPlayer code passes
    # const char**. Keep input bytes unmodified but give iconv its own pointer.
    patch_file(root, "source/mplayer/mp_msg.c",
               lambda x: replace_once(
                   replace_once(x,
                     "    precoded = recoded_filename;",
                     "    precoded = recoded_filename;\n"
                     "    char *mutable_filename = (char *)filename;",
                     "WiiMC iconv input pointer"),
                   "iconv(inv_msgiconv, &filename, &filename_len,",
                   "iconv(inv_msgiconv, &mutable_filename, &filename_len,",
                   "WiiMC iconv mutable pointer"))
    # Original WiiMC defaults to NTFS and EXT2, whose obsolete portlibs are
    # absent from the pinned toolchain. FAT32/ISO/SMB remain enabled.
    patch_file(root, "Makefile",
               lambda x: replace_once(
                   replace_once(x, "ENABLE_NTFS                   = 1",
                                "ENABLE_NTFS                   = 0",
                                "WiiMC disable unavailable NTFS dependency"),
                   "ENABLE_EXT2                   = 1",
                   "ENABLE_EXT2                   = 0",
                   "WiiMC disable unavailable EXT2 dependency"))
    patch_file(root, "source/filebrowser.h",
               lambda x: replace_once(
                   x, "#include <gccore.h>",
                   "#include <gccore.h>\n#ifndef MAXPATHLEN\n#define MAXPATHLEN 1024\n#endif",
                   "WiiMC legacy MAXPATHLEN definition"))
    patch_file(root, "source/mplayer/osdep/gx_supp.c",
               lambda x: replace_once(
                   replace_once(x, "extern timerFadeBlack;",
                                "extern int timerFadeBlack;",
                                "WiiMC timerFadeBlack declaration"),
                   "void TakeScreenshot();",
                   "void TakeScreenshot();\nextern void wiiPause(void);",
                   "WiiMC wiiPause declaration"))

    patch_file(root, "source/mplayer/osdep/ave-rvl.c",
               lambda x: replace_once(x, "#include <gccore.h>",
                                      "#include <gccore.h>\n#include <string.h>\nextern void udelay(int us);",
                                      "WiiMC modern C compiler declarations"))


def patch_usbloader(root: Path):
    install_inc(root, "source")
    def change(s):
        s = replace_once(s,'#include "input.h"', '#include "input.h"\n#include "maizo_hid.inc"', "USBLoader")
        s = fix_pad_update(s, "\tWPAD_ScanPads();\n\tPAD_ScanPads();", False)
        for prop,fn in (("substickX","PAD_SubStickX"),("substickY","PAD_SubStickY")):
            shim = "maizo_hid_"+prop
            s = replace_once(s,f"userInput[i].pad.{prop} = {fn}(i);",
                             f"userInput[i].pad.{prop} = {shim}(i, {fn}(i));", prop)
        return s
    patch_file(root,"source/input.cpp",change)

def patch_wiisx(root: Path):
    install_inc(root, "Gamecube/gc_input")
    def change(s):
        s=replace_once(s,'#include "controller.h"', '#include "controller.h"\n#include "maizo_hid.inc"', "WiiSXRX")
        s=replace_once(s, "static int _GetKeys(int Control, BUTTONS * Keys, controller_config_t* config)\n{",
                       "static int _GetKeys(int Control, BUTTONS * Keys, controller_config_t* config)\n{\n"
                       "\tmaizo_hid_tick();\n\tif(maizo_hid_connected(0)) gc_connected |= 1;", "WiiSXRX keys")
        s=replace_once(s, "unsigned int b = PAD_ButtonsHeld(Control);",
                       "unsigned int b = PAD_ButtonsHeld(Control) | maizo_hid_held(Control);", "WiiSXRX buttons")
        # GC availability is overwritten by PAD_ScanPads; restore HID bit AFTER it.
        scan='if(padNeedScan){ gc_connected = PAD_ScanPads(); padNeedScan = 0; }'
        if s.count(scan)!=2:
            raise ValueError("WiiSXRX expected two PAD scan sites, got "+str(s.count(scan)))
        s=s.replace(scan, scan+'\n\tif(maizo_hid_connected(0))gc_connected |= 1;')
        # Preserve original GC analog input when a HID pad is not connected.
        for native,shim in (("PAD_StickX","maizo_hid_stickX"),
                             ("PAD_StickY","maizo_hid_stickY"),
                             ("PAD_SubStickX","maizo_hid_substickX"),
                             ("PAD_SubStickY","maizo_hid_substickY")):
            s=s.replace(native+"(Control)",shim+"(Control, "+native+"(Control))")
        s=replace_once(s,"static void refreshAvailable(void){",
                       "static void refreshAvailable(void){\n\tmaizo_hid_tick();\n"
                       "\tif(maizo_hid_connected(0))gc_connected |= 1;", "WiiSXRX availability")
        return s
    patch_file(root, "Gamecube/gc_input/controller-GC.c", change)
    # Preserve the exception-spec syntax used by this legacy WiiSXRX codebase.
    patch_file(root, "Gamecube/Makefile_Wii",
               lambda x: replace_once(
                   replace_once(x, "CFLAGS\t= -g -flto",
                                "CFLAGS\t= -fgnu89-inline -fcommon -g -flto",
                                "WiiSXRX legacy GCC C inline and common symbol semantics"),
                   "CXXFLAGS\t=\t$(CFLAGS)",
                   "CXXFLAGS\t=\t$(CFLAGS) -std=gnu++14", "WiiSXRX C++ standard"))

    # Modern GCC no longer accepts implicit function prototypes and the
    # old BOOL typedef is not present in newer libogc headers.
    patch_file(root, "Gamecube/gc_input/controller.h",
               lambda x: replace_once(x, "BOOL          inUse;", "bool          inUse;", "WiiSXRX BOOL"))
    # The modern compiler does not treat an empty-parameter declaration as
    # a prototype, and CheckState(char *) clashes with legacy CheckState().
    # libogc provides static inline gettime/diff_usec in the watchdog header.
    # The old Peops FPS code only declared externs, causing linker failures.
    patch_file(root, "PeopsSoftGPU/fps.c",
               lambda x: replace_once(x,
                   " long long gettime(void);\n"
                   " unsigned int diff_usec(long long start,long long end);",
                   " #include <ogc/lwp_watchdog.h>",
                   "WiiSXRX Peops FPS libogc timebase"))
    patch_file(root, "misc.h",
               lambda x: replace_once(x, "int CheckState();",
                                      "int CheckState(char *file);",
                                      "WiiSXRX CheckState prototype"))

    patch_file(root, "Gamecube/PlugCD.h",
               lambda x: replace_once(x, "unsigned char* getSector();",
                                      "unsigned char* getSector(int subchannel);", "WiiSXRX getSector"))

    patch_file(root, "Gamecube/fileBrowser/fileBrowser-CARD.c",
               lambda x: replace_once(x, "memalign(32,CARD_WORKAREA)", 
                                      "memalign(32,CARD_WORKAREA_SIZE)", "WiiSXRX modern CARD size"))

    patch_file(root, "cdrom.c",
               lambda x: replace_once(x, '#include "cdrom.h"',
                                      '#include "cdrom.h"\n#ifndef TRUE\n#define TRUE 1\n#endif\n#ifndef FALSE\n#define FALSE 0\n#endif',
                                      "WiiSXRX legacy boolean defines"))


MAP_GENERIC = '''
static uint32_t MaizoHidToGeneric(uint32_t b) {
 uint32_t o = INPUT_BTN_NONE;
 if(b&PAD_BUTTON_A)o|=INPUT_BTN_A;
 if(b&PAD_BUTTON_B)o|=INPUT_BTN_B;
 if(b&PAD_BUTTON_X)o|=INPUT_BTN_X;
 if(b&PAD_BUTTON_Y)o|=INPUT_BTN_Y;
 if(b&PAD_BUTTON_UP)o|=INPUT_BTN_UP;
 if(b&PAD_BUTTON_DOWN)o|=INPUT_BTN_DOWN;
 if(b&PAD_BUTTON_LEFT)o|=INPUT_BTN_LEFT;
 if(b&PAD_BUTTON_RIGHT)o|=INPUT_BTN_RIGHT;
 if(b&PAD_BUTTON_START)o|=INPUT_BTN_PLUS;
 if(b&PAD_TRIGGER_Z)o|=INPUT_BTN_MINUS;
 if(b&PAD_TRIGGER_L)o|=INPUT_TRIGGER_L;
 if(b&PAD_TRIGGER_R)o|=INPUT_TRIGGER_R;
 return o;
}
'''

def patch_gx(root: Path):
    install_inc(root,"source/drivers/ogc/wii")
    def change(s):
        s=replace_once(s,'#include "WiiInputDriver.h"',
                       '#include "WiiInputDriver.h"\n#include "maizo_hid.inc"', "GX Input")
        s=replace_once(s, "void WiiInputDriver::scanHardware() {\n\tWPAD_ScanPads();",
                       "void WiiInputDriver::scanHardware() {\n\tWPAD_ScanPads();\n\tmaizo_hid_tick();", "GX scan")
        marker="void WiiInputDriver::readChannel(int channel, InputPadData & pad) {"
        inject=MAP_GENERIC+'''
void WiiInputDriver::readChannel(int channel, InputPadData & pad) {
 if(channel==0 && maizo_hid_connected(0)){
  pad.hw_connected[INPUT_HW_GAMECUBE] = true;
  pad.hw_buttons_d[INPUT_HW_GAMECUBE] |= MaizoHidToGeneric(maizo_hid_down(0));
  pad.hw_buttons_h[INPUT_HW_GAMECUBE] |= MaizoHidToGeneric(maizo_hid_held(0));
  pad.hw_buttons_r[INPUT_HW_GAMECUBE] |= MaizoHidToGeneric(maizo_hid_up(0));
  pad.hw_stickX[INPUT_HW_GAMECUBE] = maizo_hid_stickX(0,0)/96.0f;
  pad.hw_stickY[INPUT_HW_GAMECUBE] = maizo_hid_stickY(0,0)/96.0f;
 }
'''
        return replace_once(s, marker, inject, "GX channel")
    patch_file(root,"source/drivers/ogc/wii/WiiInputDriver.cpp", change)

def patch_snes(root: Path):
    install_inc(root,"source/drivers/ogc/wii/input")
    def change(s):
        s=replace_once(s,'#include "usbinput.h"', '#include "usbinput.h"\n#include "maizo_hid.inc"', "SNES")
        s=replace_once(s, "UsbInput_Scan();", "UsbInput_Scan();\n\tmaizo_hid_tick();","SNES scan")
        s=replace_once(s,
          "bool active = retrodeActive || (hornetActive && channel == 0) || (mayflashActive && channel < 2);",
          "bool active = retrodeActive || (hornetActive && channel == 0) || "
          "(mayflashActive && channel < 2) || maizo_hid_connected(channel);", "SNES active")
        s=replace_once(s,
          "uint32_t held = Retrode_ButtonsHeld(channel) | Hornet_ButtonsHeld(channel) | Mayflash_ButtonsHeld(channel);",
          "uint32_t held = Retrode_ButtonsHeld(channel) | Hornet_ButtonsHeld(channel) | "
          "Mayflash_ButtonsHeld(channel) | maizo_hid_held(channel);", "SNES union")
        return s
    patch_file(root,"source/drivers/ogc/wii/input/WiiUsbPads.cpp", change)

MODULES = {
  "WiiMC": patch_wiimc,
  "usbloader_gx": patch_usbloader,
  "WiiSXRX": patch_wiisx,
  "snes9xgx": patch_snes,
  "fceugx": patch_gx,
  "vbagx": patch_gx,
}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("module", choices=sorted(MODULES))
    p.add_argument("source", type=Path, help="clean pinned upstream checkout")
    args=p.parse_args()
    if not args.source.is_dir():
        raise SystemExit("Missing upstream checkout: " + str(args.source))
    MODULES[args.module](args.source)
    print("SOURCE PATCHED (compile and Wii hardware validation still required):",args.module)

if __name__ == "__main__":
    main()
