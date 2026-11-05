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
    # WiiMC 3.0 ships its large menu.cpp as Latin-1 (e.g. 0xe7 for ç).
    # Do not decode it as UTF-8 or silently change non-ASCII UI resources.
    try:
        codec = "utf-8"
        original = original_bytes.decode(codec)
    except UnicodeDecodeError:
        codec = "latin-1"  # byte-preserving for pinned legacy C/C++ sources
        original = original_bytes.decode(codec)
    original = original.replace("\r\n", "\n")
    updated = fun(original)
    if original == updated:
        raise ValueError("Patch produced no changes: " + rel)
    path.write_bytes(updated.replace("\n", eol).encode(codec))
    print("PATCH", rel)

def install_inc(root: Path, folder: str):
    target = root / folder
    target.mkdir(parents=True, exist_ok=True)
    for name in ("maizo_hid.inc", "maizo_report.h", "maizo_multi.h"):
        shutil.copyfile(SOURCE.parent / name, target / name)

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
        # A USB bus rescan during video decode can collide with the USB
        # mass-storage player under IOS58. Only poll an already-attached HID
        # during playback; discover new controllers in the normal WiiMC GUI.
        s = replace_once(s, "\tmaizo_hid_tick();",
            "\tif(!wiiIsPlaying() || maizo_hid_connected(0) || "
            "maizo_hid_connected(1)) maizo_hid_tick();",
            "WiiMC USB HID non-invasive playback polling")
        # DrawMPlayerGui calls UpdatePads immediately before MPlayerInput.
        # Read the same snapshot: calling PAD_ButtonsDown a second time can
        # lose HID edges and mismatch GUI/video controls.
        s = replace_once(s, "u16 down = PAD_ButtonsDown(cur_chan);",
                         "u16 down = userInput[cur_chan].pad.btns_d;", "MPlayerInput snapshot down")
        s = replace_once(s, "u16 held = PAD_ButtonsHeld(cur_chan);",
                         "u16 held = userInput[cur_chan].pad.btns_h;", "MPlayerInput snapshot held")
        # Channel switching previously reverted to the native pad only.
        if (s.count("held = PAD_ButtonsDown(cur_chan);") != 2 or
                s.count("down = PAD_ButtonsDown(cur_chan);") != 2):
            raise ValueError("Unexpected WiiMC channel switching layout")
        s = s.replace("held = PAD_ButtonsDown(cur_chan);",
                      "held = userInput[cur_chan].pad.btns_h;")
        s = s.replace("down = PAD_ButtonsDown(cur_chan);",
                      "down = userInput[cur_chan].pad.btns_d;")
        return s
    patch_file(root, "source/input.cpp", input_patch)
    def wiimc_autoplay(s):
        s = replace_once(s, "sprintf(loadedFile, argv[1]);",
                         'snprintf(loadedFile, sizeof(loadedFile), "%s", argv[1]);',
                         "WiiMC argv format")
        # Validate argv media AFTER WiiMC has mounted usb1:/sd1:/. A
        # frontend DOL boot can succeed while the player uses a different
        # FAT partition alias, and upstream immediately blacks the screen.
        # Preserve the original engine; simply reject unreadable files and
        # search mounted USB partitions rather than hanging on a bad path.
        s = replace_once(s,
            "FindAppPath(); // Initialize SD and USB devices and look for apps/wiimc",
            """FindAppPath(); // Initialize SD and USB devices and look for apps/wiimc
    if(loadedFile[0] != 0 &&
       (!strncmp(loadedFile, "usb", 3) || !strncmp(loadedFile, "sd", 2))) {
        FILE *mediaCheck = fopen(loadedFile, "rb");
        if(!mediaCheck && !strncmp(loadedFile, "usb1:/", 6)) {
            char original[sizeof(loadedFile)];
            snprintf(original,sizeof(original),"%s",loadedFile);
            for(int volume=2;volume<=5 && !mediaCheck;volume++) {
                snprintf(loadedFile,sizeof(loadedFile),
                         "usb%d:/%s",volume,original+6);
                mediaCheck = fopen(loadedFile,"rb");
            }
            if(!mediaCheck)
                snprintf(loadedFile,sizeof(loadedFile),"%s",original);
        }
        if(mediaCheck) {
            fclose(mediaCheck);
            printf("MAIZO WIIMC 3.0: media OK: %s\\n",loadedFile);
        } else {
            printf("MAIZO WIIMC 3.0: media nao montada: %s\\n",loadedFile);
            loadedFile[0] = 0; // show native GUI; never silently black out
        }
    }""",
            "WiiMC media path preflight after FAT mount")
        # WiiMC deliberately sets AutobootExit on argv playback and then
        # reboots when the movie ends. Keep the working player but return
        # to its normal browser / playback GUI instead of resetting Wii.
        return replace_once(
            s, 'snprintf(loadedFile, sizeof(loadedFile), "%s", argv[1]);\n\t\tAutobootExit = true;',
            'snprintf(loadedFile, sizeof(loadedFile), "%s", argv[1]);\n\t\tAutobootExit = false;',
            "WiiMC do not reboot at video EOF")
    patch_file(root, "source/wiimc.cpp", wiimc_autoplay)
    # Original 3.0 playback worker can remain in status 2/1 indefinitely if
    # it fails to enter the codec engine. Bound these waits and make the
    # existing GUI recover. This is a defensive timeout, not codec support.
    def player_deadlock_guards(t):
        t = replace_once(t,
            "\tmplayerstack=(u8*)(memalign(32,MPLAYER_STACKSIZE*sizeof(u8)));\n"
            "\tmemset(mplayerstack,0,MPLAYER_STACKSIZE*sizeof(u8));\n"
            "\tLWP_CreateThread (&mthread, mplayerthread, NULL, mplayerstack, MPLAYER_STACKSIZE, 68);",
            "\tmplayerstack=(u8*)(memalign(32,MPLAYER_STACKSIZE*sizeof(u8)));\n"
            "\tif(!mplayerstack){printf(\"MAIZO WIIMC: no MPlayer stack\\n\");return false;}\n"
            "\tmemset(mplayerstack,0,MPLAYER_STACKSIZE*sizeof(u8));\n"
            "\tif(LWP_CreateThread(&mthread,mplayerthread,NULL,mplayerstack,MPLAYER_STACKSIZE,68)<0){\n"
            "\t\tprintf(\"MAIZO WIIMC: thread creation failed\\n\");\n"
            "\t\tfree(mplayerstack);return false;\n"
            "\t}",
            "WiiMC handle failed MPlayer thread init")
        t = replace_once(t,
            "\twhile(controlledbygui == 2)\n\t\tusleep(100);",
            "\tu64 maizoOldPlayerWait=gettime();\n"
            "\twhile(controlledbygui==2 && diff_sec(maizoOldPlayerWait,gettime())<20)\n"
            "\t\tusleep(1000);\n"
            "\tif(controlledbygui==2){\n"
            "\t\tprintf(\"MAIZO WIIMC: previous player never released control\\n\");\n"
            "\t\tcontrolledbygui=1;ResumeDeviceThread();return;\n"
            "\t}",
            "WiiMC cap dead previous decoder")
        t = replace_once(t,
            "\twhile(controlledbygui != 0)\n\t\tusleep(100);",
            "\tu64 maizoStartWait=gettime();\n"
            "\twhile(controlledbygui!=0 && diff_sec(maizoStartWait,gettime())<25)\n"
            "\t\tusleep(1000);\n"
            "\tif(controlledbygui!=0){\n"
            "\t\tprintf(\"MAIZO WIIMC: player did not start within 25s\\n\");\n"
            "\t\tcontrolledbygui=1;ResumeDeviceThread();\n"
            "\t}",
            "WiiMC cap dead startup/unsupported codec")
        return t
    patch_file(root, "source/wiimc.cpp", player_deadlock_guards)
    # In 3.0 tiled-video mode, MPlayerInput is skipped without a valid
    # Wiimote IR pointer, even with gamepad buttons pressed. Keep the old
    # video renderer and explicitly honor PAD/HID snapshots while tiled.
    def video_hid(t):
        old = ("else if (!wiiTiledRender)\n"
               "\t\tMPlayerInput();\n"
               "\telse if(point_on)\n"
               "\t\tMPlayerInput();")
        new = ("else if (!wiiTiledRender || point_on ||\n"
               "\t         userInput[0].pad.btns_d || userInput[0].pad.btns_h ||\n"
               "\t         userInput[1].pad.btns_d || userInput[1].pad.btns_h)\n"
               "\t\tMPlayerInput();")
        return replace_once(t,old,new,"WiiMC 3.0 HID control during tiled video")
    patch_file(root, "source/video.cpp", video_hid)

    # Preserve the original 3.0 interface, but do not black out the TV
    # before first-frame success. On a missing/stalled MPlayer, the menu is
    # still visible and can report a failure rather than appearing bricked.
    def gui_autoplay_recovery(t):
        t = replace_once(t,
            "if(loadedFile[0] != 0)\n\t\t{\n\t\t\tVIDEO_SetBlack(TRUE);\n\t\t}",
            "if(loadedFile[0] != 0)\n\t\t{\n\t\t\tVIDEO_SetBlack(FALSE);\n\t\t}",
            "WiiMC show GUI before autoplay")
        t = replace_once(t,
            "\t\t\tguiShutdown = true;\n\t\t\tsleep(2); // USB mount delay\n\n"
            "\t\t\tLoadNewFile();\n\n"
            "\t\t\tSuspendParseThread();\n\t\t\tSuspendGui();",
            "\t\t\tguiShutdown = true;\n\t\t\tsleep(2); // USB mount delay\n\n"
            "\t\t\tint launched = LoadNewFile();\n"
            "\t\t\tif(launched == 1 && controlledbygui == 0) {\n"
            "\t\t\t\tSuspendParseThread();\n\t\t\t\tSuspendGui();\n"
            "\t\t\t} else {\n"
            "\t\t\t\tguiShutdown = false;\n"
            "\t\t\t\tloadedFile[0] = 0;\n"
            "\t\t\t\tprintf(\"MAIZO WIIMC: playback aborted, showing GUI.\\\\n\");\n"
            "\t\t\t}",
            "WiiMC fail-open autoboot UI")
        return t
    patch_file(root, "source/menu.cpp", gui_autoplay_recovery)

    # This legacy MPlayer subtitle tag attempted to call libogc pad motor
    # from the isolated MPlayer C build without its header. Discard only
    # this nonessential subtitle-triggered rumble, preserving playback.
    patch_file(root, "source/mplayer/libass/ass_parse.c",
               lambda s: replace_once(
                   replace_once(s, "PAD_ControlMotor(0, i);", "(void)i;",
                                "WiiMC subtitle motor effect"),
                   "PAD_ControlMotor(0, 0);", "(void)0;",
                   "WiiMC subtitle motor reset"))
    # libass still calls memalign(), which modern GCC requires a prototype for.
    patch_file(root, "source/mplayer/libass/ass_render.c",
               lambda s: replace_once(s, "#include <math.h>",
                                      "#include <math.h>\n#include <malloc.h>",
                                      "WiiMC libass aligned allocator prototype"))
    # GEKKO rmff.h deliberately excludes POSIX sockets; the Wii API is
    # net_recv, not an implicit recv(int,void*,size_t,int) declaration.
    patch_file(root, "source/mplayer/stream/realrtsp/rmff.c",
               lambda s: replace_once(s, '#include "rmff.h"',
                                      '#include "rmff.h"\n#ifdef GEKKO\n'
                                      '#include <network.h>\n#define recv net_recv\n#endif',
                                      "WiiMC RealRTSP Wii network recv"))
    patch_file(root, "source/mplayer/stream/cache2.c",
               lambda s: replace_once(
                   replace_once(s, '#include "stream.h"',
                                '#include "stream.h"\n#ifndef MAXPATHLEN\n'
                                '#define MAXPATHLEN 1024\n#endif',
                                "WiiMC cache2 MAXPATHLEN"),
                   'if(global_buffer==NULL) global_buffer=0x90002000;',
                   'if(global_buffer==NULL) global_buffer=(unsigned char *)0x90002000u;',
                   "WiiMC cache2 fixed MEM2 pointer type"))
    # Old FFmpeg was configured before libogc gained POSIX sockaddr_storage
    # and addrinfo. Avoid redeclaring those native Wii structures.
    def ffmpeg_network(t):
        t = replace_once(t, '#if !HAVE_STRUCT_SOCKADDR_STORAGE',
                         '#if !HAVE_STRUCT_SOCKADDR_STORAGE && !defined(GEKKO)',
                         "WiiMC ffmpeg sockaddr_storage")
        return replace_once(t, '#if !HAVE_STRUCT_ADDRINFO',
                            '#if !HAVE_STRUCT_ADDRINFO && !defined(GEKKO)',
                            "WiiMC ffmpeg addrinfo")
    patch_file(root, "source/mplayer/ffmpeg/libavformat/network.h", ffmpeg_network)
    # This old PPC paired-single header implements its own psq_* intrinsics.
    # A redundant legacy devkitPro <paired.h> was removed from modern SDKs.
    patch_file(root, "source/mplayer/ffmpeg/libavutil/ppc/paired.h",
               lambda s: replace_once(s, "#include <paired.h>",
                                      "/* paired.h legacy SDK dependency removed */",
                                      "WiiMC inline paired PPC intrinsics"))
    # Modern devkitPPC no longer supports the old -mpaired extension.
    # The software DSP fallback is safe on Broadway; enable paired-single
    # acceleration only when a supported compiler/toolchain is available.
    patch_file(root, "source/mplayer/config.h",
               lambda s: replace_once(s, "#define HAVE_PAIRED 1",
                                      "#define HAVE_PAIRED 0",
                                      "WiiMC modern PPC paired fallback"))
    patch_file(root, "source/mplayer/config.mak",
               lambda s: replace_once(s, "HAVE_PAIRED = yes",
                                      "HAVE_PAIRED = no\n"
                                      "CFLAGS += -Wno-error=incompatible-pointer-types",
                                      "WiiMC disable paired and accept historical swscale callback types"))
    def patch_bink(s):
        if s.count("#ifdef HAVE_PAIRED") != 2:
            raise ValueError("Unexpected Bink paired-CPU compile guards")
        s = s.replace("#ifdef HAVE_PAIRED", "#if HAVE_PAIRED")
        return replace_once(
            s, "if (HAVE_PAIRED) c->scale_block = scale_block_paired;",
            "#if HAVE_PAIRED\n    c->scale_block = scale_block_paired;\n#endif",
            "WiiMC Bink legacy accelerated function absent on modern GCC")
    patch_file(root, "source/mplayer/ffmpeg/libavcodec/binkdsp.c", patch_bink)
    patch_file(root, "source/mplayer/ffmpeg/libavcodec/h264pred.c",
               lambda s: replace_once(
                   s, "if (HAVE_PAIRED) ff_h264_pred_init_ppc(h, codec_id, bit_depth, chroma_format_idc);",
                   "#if HAVE_PAIRED\n    ff_h264_pred_init_ppc(h, codec_id, bit_depth, chroma_format_idc);\n#endif",
                   "WiiMC old H264 paired disabled on modern devkitPPC"))

    # Generated config.h has HAVE_PAIRED=0; C runtime 'if(0)' still references
    # undeclared paired functions because C compilation type-checks dead code.
    for source, oldcall in (
        ("fft.c", "if (HAVE_PAIRED)  ff_fft_init_paired(s);"),
        ("rdft.c", "if (HAVE_PAIRED) ff_rdft_init_paired(s);"),
        ("vc1dsp.c", "if (HAVE_PAIRED)\n        ff_vc1dsp_init_paired(dsp);"),
    ):
        patch_file(root, "source/mplayer/ffmpeg/libavcodec/" + source,
                   lambda s, oldcall=oldcall: replace_once(
                       s, oldcall, "#if HAVE_PAIRED\n    " +
                       oldcall.replace("if (HAVE_PAIRED)", "").strip() +
                       "\n#endif", "WiiMC guarded PPC paired init " + source))
    # Modern devkitPPC exports __app_start, whereas this legacy linker
    # script expects _start. Keep WiiMC's USB stub without dropping CRT0.
    def modern_wiimc_linker(s):
        s = replace_once(s, "EXTERN(_start);", "EXTERN(__app_start);",
                         "WiiMC modern entry symbol")
        s = replace_once(s, "ENTRY(_start);", "ENTRY(__app_start);",
                         "WiiMC modern ENTRY directive")
        return replace_once(
            s, "KEEP (*crt0.o(*.init))",
            "KEEP (*(.crt0 .crt0.*))\n\t\tKEEP (*crt0.o(*.init))",
            "WiiMC modern crt0 section")
    patch_file(root, "build/wiimc.ld", modern_wiimc_linker)
    # Old header defined four globals in every C file; GCC -fno-common
    # rejects the resulting duplicates. Provide one definition in mplayer.c.
    def extern_mplayer_paths(s):
        for name in ("DATADIR", "CONFDIR", "LIBDIR", "CSSDIR"):
            s=replace_once(s, "char MPLAYER_"+name+"[512]; ",
                           "extern char MPLAYER_"+name+"[512];",
                           "WiiMC MPlayer common variable "+name)
        return s
    patch_file(root, "source/mplayer/config.h", extern_mplayer_paths)
    def mplayer_path_owner(s):
        return replace_once(
            s, '#include "stream/stream.h"',
            '#include "stream/stream.h"\n'
            'char MPLAYER_DATADIR[512]={0};\n'
            'char MPLAYER_CONFDIR[512]={0};\n'
            'char MPLAYER_LIBDIR[512]={0};\n'
            'char MPLAYER_CSSDIR[512]={0};',
            "WiiMC single owner MPlayer paths")
    patch_file(root, "source/mplayer/mplayer.c", mplayer_path_owner)

    # Legacy libfat exposed the volume label accessor, but the current Wii
    # library no longer links that API. Keep FAT mounting, omit optional label.
    patch_file(root, "source/fileop.cpp",
               lambda s: replace_once(
                   s, "fatGetVolumeLabel(mount, part[device][*devnum].name);",
                   "part[device][*devnum].name[0] = 0;",
                   "WiiMC FAT volume label optional"))
    # The VI encoder needs 2-4 microsecond waits; use libogc watchdog ticks
    # rather than introducing a millisecond-granularity usleep() regression.
    def vi_delay_compat(s):
        if s.count("udelay(") < 5:
            raise ValueError("Unexpected VI encoder microsecond delay layout")
        s=s.replace("udelay(", "maizo_udelay(")
        return replace_once(s, "extern void maizo_udelay(int us);",
            "#include <ogc/lwp_watchdog.h>\n"
            "static void maizo_udelay(int us){\n"
            " u64 start=gettime();\n"
            " while(diff_usec(start,gettime()) < (u32)us){}\n"
            "}", "WiiMC unique watchdog VI timing")
    patch_file(root, "source/utils/vi_encoder.c", vi_delay_compat)
    # Modern bin2o does not produce the old *_size linker globals. Emit
    # actual sizes from the pinned assets, with NO changes to asset contents.
    asset_sources=[]
    for folder,exts in (("fonts",(".ttf",)),("images",(".jpg",".png")),
                        ("lang",(".lang",))):
        for file in sorted((root/"source"/folder).iterdir()):
            if file.is_file() and file.suffix.lower() in exts:
                name="".join(c if c.isalnum() else "_" for c in file.name)
                asset_sources.append(
                    "unsigned int "+name+"_size="+str(file.stat().st_size)+"u;")
    if not asset_sources:
        raise ValueError("WiiMC assets missing from pinned source")
    (root/"source/utils/maizo_asset_sizes.c").write_text(
        "/* Actual sizes of unmodified WiiMC assets for modern bin2o. */\n"+
        "\n".join(asset_sources)+"\n",encoding="utf-8")
    # libmxml's pinned old precompiled archive still references newlib's
    # retired __ctype_ptr__ (the old 257-byte ASCII/EOF classifier). Provide
    # its ABI-compatible lookup table without touching the original library.
    flags=[]
    for code in range(256):
        bits=0
        if code<32 or code==127: bits|=0x20
        if code in (9,10,11,12,13,32): bits|=0x08
        if code==32: bits|=0x80
        if code in (33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
                     58,59,60,61,62,63,64,91,92,93,94,95,96,123,124,125,126):
            bits|=0x10
        if 48<=code<=57:bits|=0x04|0x40
        if 65<=code<=90:bits|=0x01
        if 97<=code<=122:bits|=0x02
        if 65<=code<=70 or 97<=code<=102:bits|=0x40
        flags.append(bits)
    table=",".join(str(v) for v in [0]+flags)
    (root/"source/utils/maizo_legacy_ctype.c").write_text(
        "/* Legacy newlib ABI for original mxml binary; ASCII classification. */\n"
        "static unsigned char maizo_ctype_table[257]={"+table+"};\n"
        "char *__ctype_ptr__=(char *)maizo_ctype_table;\n"
        "/* Deprecated external udelay ABI used by pinned MPlayer archive. */\n"
        "#include <ogc/lwp_watchdog.h>\n"
        "#include <ogc/cache.h>\n"
        "/* Pinned 3.0 MPlayer archive references pre-modern libogc C symbols.\n"
        " * Resolve their old names through supported modern libogc APIs. */\n"
        "u64 maizo_compat_gettime(void) __asm__(\"gettime\");\n"
        "u64 maizo_compat_gettime(void){return gettime();}\n"
        "void maizo_compat_dcflush(void *p,u32 n) __asm__(\"DCFlushRange\");\n"
        "void maizo_compat_dcflush(void *p,u32 n){DCFlushRange(p,n);}\n"
        "void maizo_compat_dcstore(void *p,u32 n) __asm__(\"DCStoreRangeNoSync\");\n"
        "void maizo_compat_dcstore(void *p,u32 n){DCStoreRangeNoSync(p,n);}\n"
        "void maizo_compat_udelay(unsigned us) __asm__(\"udelay\");\n"
        "void maizo_compat_udelay(unsigned us){\n"
        " u64 start=gettime();\n"
        " while(diff_usec(start,gettime())<us){}\n"
        "}\n",
        encoding="utf-8")

    # Pinned historical MPlayer has version.sh without its executable bit.
    # Run through /bin/sh rather than trusting a checkout's file mode.
    patch_file(root, "source/mplayer/Makefile",
               lambda s: replace_once(s, "./$< `$(CC) -dumpversion`",
                                      "sh ./$< `$(CC) -dumpversion`",
                                      "WiiMC MPlayer non-executable version script"))

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
    # Keep ORIGINAL WiiMC 3.0 media engine/FFmpeg archives, already used
    # by the working upstream release. Only rebuild GUI/input glue with HID.
    # Rebuilding FFmpeg/MPlayer with modern GCC introduced the black-screen
    # regression despite green CI. The pinned archives include paired DSP.
    # Do not silently relink a different codec engine.
    original_libs = (
        "source/mplayer/libmplayerwii.a",
        "source/mplayer/ffmpeg/libavformat/libavformat.a",
        "source/mplayer/ffmpeg/libavcodec/libavcodec.a",
        "source/mplayer/ffmpeg/libswscale/libswscale.a",
        "source/mplayer/ffmpeg/libavutil/libavutil.a",
    )
    for rel in original_libs:
        f = root / rel
        if not f.is_file() or f.stat().st_size < 100000:
            raise ValueError("Missing original WiiMC 3.0 playback library: " + rel)
    # Makefile links archives from the pinned repo. Never run nested MPlayer
    # make unless specifically regenerating the whole original codec engine.
    def retain_original_mplayer(s):
        return replace_once(
            s, "cd source/mplayer; $(MAKE) -f Makefile; cd ../..",
            "@test -s source/mplayer/libmplayerwii.a && "
            "test -s source/mplayer/ffmpeg/libavcodec/libavcodec.a && "
            "test -s source/mplayer/ffmpeg/libavformat/libavformat.a && "
            "test -s source/mplayer/ffmpeg/libswscale/libswscale.a && "
            "test -s source/mplayer/ffmpeg/libavutil/libavutil.a",
            "WiiMC ORIGINAL 3.0 media engine retained")
    patch_file(root, "Makefile", retain_original_mplayer)

    def fileop_compat(t):
        t = replace_once(t, "#include <string.h>",
                         "#include <string.h>\n#include <strings.h>\n#define stricmp strcasecmp",
                         "WiiMC portable stricmp")
        return replace_once(t, 'strstr(path, "../") > 0',
                            'strstr(path, "../") != NULL',
                            "WiiMC pointer comparison")
    patch_file(root, "source/fileop.cpp", fileop_compat)

    def vf_scale_compat(t):
        t = replace_once(t, "src2[1]= pal2;",
                         "src2[1]= (uint8_t *)pal2;",
                         "WiiMC BE palette cast")
        for old,new in (
            ("sws_scale(sws1, src2, src_stride2,",
             "sws_scale(sws1, (const uint8_t * const *)src2, src_stride2,"),
            ("sws_scale(sws2, src2, src_stride2,",
             "sws_scale(sws2, (const uint8_t * const *)src2, src_stride2,"),
            ("sws_scale(sws1, src2, src_stride,",
             "sws_scale(sws1, (const uint8_t * const *)src2, src_stride,")):
            t = replace_once(t, old, new, "WiiMC swscale legacy source-const")
        return t
    patch_file(root, "source/mplayer/libmpcodecs/vf_scale.c", vf_scale_compat)
    patch_file(root, "source/mplayer/sub/spudec.c",
               lambda x: replace_once(
                   replace_once(x, "sws_scale(ctx,&s1,",
                                "sws_scale(ctx,(const uint8_t * const *)&s1,",
                                "WiiMC subtitle swscale first call"),
                   "sws_scale(ctx,&s2,",
                   "sws_scale(ctx,(const uint8_t * const *)&s2,",
                   "WiiMC subtitle swscale second call"))
    def wiimc_callbacks(t):
        t = replace_once(t,
                         "static ssize_t __out_write(struct _reent *r, int fd,",
                         "static ssize_t __out_write(struct _reent *r, void *fd,",
                         "WiiMC newlib devoptab output callback")
        t = replace_once(t, "static void ResetCB()",
                         "static void ResetCB(u32 irq, void *userdata)",
                         "WiiMC libogc reset callback signature")
        return t
    patch_file(root, "source/wiimc.cpp", wiimc_callbacks)
    def wii_subtitles_compat(t):
        t = replace_once(t, 'c = utf8_get_char(&t);',
                         '{ const char *cursor=(const char *)t; '
                         'c = utf8_get_char(&cursor); '
                         't = (unsigned char *)cursor; }',
                         "WiiMC subtitle UTF8 input pointer")
        return replace_once(t, '#include "sub.h"',
                            '#include "sub.h"\n#ifdef GEKKO\n'
                            'int mpctx_get_set_of_sub_size(void);\n#endif',
                            "WiiMC subtitle settings prototype")
    patch_file(root, "source/mplayer/sub/sub.c", wii_subtitles_compat)
    def gui_image_compat(t):
        t=replace_once(t, 'static u64 prev;',
                       'static u64 rotate_prev;', "WiiMC GUI rotate state")
        t=replace_once(t, 'diff_usec(prev, now)',
                       'diff_usec(rotate_prev, now)', "WiiMC GUI rotate elapsed")
        return replace_once(t, 'prev = now;',
                            'rotate_prev = now;', "WiiMC GUI rotate update")
    patch_file(root, "source/libwiigui/gui_image.cpp", gui_image_compat)
    patch_file(root, "source/mplayer/libdvdnav/dvdnav_internal.h",
               lambda x: replace_once(x,
                                      "typedef mutex_t pthread_mutex_t;",
                                      "#define pthread_mutex_t mutex_t",
                                      "WiiMC modern pthread type conflict"))
    patch_file(root, "source/libwiigui/gui_trigger.cpp",
               lambda x: (x.replace("prev[", "lastTrigger[")
                          if x.count("prev[") >= 10 else
                          (_ for _ in ()).throw(ValueError("Unexpected GUI input history layout"))))
    patch_file(root, "source/musicplaylist.cpp",
               lambda x: replace_once(x, "#include <string.h>",
                                      "#include <string.h>\n#include <strings.h>\n"
                                      "#define stricmp strcasecmp",
                                      "WiiMC portable music sort"))
    patch_file(root, "source/mplayer/loader/qtx/qtxsdk/components.h",
               lambda x: replace_once(x,
                                      "unsigned int* p=cd->wantedDestinationPixelTypes;",
                                      "unsigned int* p=(unsigned int*)(void*)"
                                      "cd->wantedDestinationPixelTypes;",
                                      "WiiMC QuickTime debug pointer cast"))
    patch_file(root, "source/settings.h",
               lambda x: replace_once(x, '#include "networkop.h"',
                                      '#include "networkop.h"\n'
                                      '#ifndef MAXPATHLEN\n#define MAXPATHLEN 1024\n#endif',
                                      "WiiMC legacy MAXPATHLEN in settings"))
    patch_file(root, "source/input.cpp",
               lambda x: (x.replace("prev[", "rumblePrev[")
                          if x.count("prev[") >= 4 else
                          (_ for _ in ()).throw(ValueError("Unexpected rumble variable layout"))))
    patch_file(root, "source/mplayer/libmpdemux/demux_film.c",
               lambda x: replace_once(x,
                                      "unsigned short* tmp = dp->buffer;",
                                      "unsigned short* tmp = (unsigned short*)dp->buffer;",
                                      "WiiMC 16-bit film audio buffer cast"))
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

    patch_file(root, "source/mplayer/libmpcodecs/vf_screenshot.c",
               lambda x: replace_once(
                   replace_once(x,
                      "sws_scale(priv->ctx, mpi->planes,",
                      "sws_scale(priv->ctx, (const uint8_t * const *)mpi->planes,",
                      "WiiMC screenshot plane const"),
                   "sws_scale(vf->priv->ctx, src,",
                   "sws_scale(vf->priv->ctx, (const uint8_t * const *)src,",
                   "WiiMC screenshot slice const"))
    def ftp_compat(t):
        t = replace_once(t, "#include <stdlib.h>",
                         "#include <stdlib.h>\n#include <stdint.h>",
                         "WiiMC FTP pointer portability")
        for name in ("st_spare1","st_spare2","st_spare3"):
            t = replace_once(t, "st->"+name+" = 0;", "/* legacy field removed: "+name+" */",
                             "WiiMC FTP struct stat compatibility")
        t = replace_once(t, "if_config(myIP, NULL, NULL, true)",
                         "if_config(myIP, NULL, NULL, true, 20)",
                         "WiiMC FTP network retry parameter")
        # newlib changed private devoptab callbacks from int fd to void*.
        # Avoid C function pointer casts: adapt the boundary explicitly.
        wrappers = '''
static int maizo_ftp_close(struct _reent*r,void*fd){
 return __ftp_close(r,(int)(uintptr_t)fd);
}
static ssize_t maizo_ftp_write(struct _reent*r,void*fd,const char*buf,size_t n){
 return __ftp_write(r,(int)(uintptr_t)fd,buf,n);
}
static ssize_t maizo_ftp_read(struct _reent*r,void*fd,char*buf,size_t n){
 return __ftp_read(r,(int)(uintptr_t)fd,buf,n);
}
static off_t maizo_ftp_seek(struct _reent*r,void*fd,off_t off,int whence){
 return __ftp_seek(r,(int)(uintptr_t)fd,off,whence);
}
static int maizo_ftp_fstat(struct _reent*r,void*fd,struct stat*st){
 return __ftp_fstat(r,(int)(uintptr_t)fd,st);
}
'''
        t=replace_once(t,"static void MountDevice(", wrappers+"\nstatic void MountDevice(",
                       "WiiMC newlib FTP adapter wrappers")
        for func in ("close","write","read","seek","fstat"):
            t=replace_once(t,"dotab_ftp->"+func+"_r=__ftp_"+func+";",
                           "dotab_ftp->"+func+"_r=maizo_ftp_"+func+";",
                           "WiiMC FTP "+func+" callback")
        return t
    patch_file(root,"source/utils/ftp_devoptab.c",ftp_compat)
    def subtitles_font_compat(t):
        t=replace_once(t,"extern const font_ttf[];",
                       "extern const unsigned char font_ttf[];",
                       "WiiMC FreeType embedded default font")
        t=replace_once(t,"extern font_mem;",
                       "extern unsigned char *font_mem;",
                       "WiiMC FreeType font cache pointer")
        return replace_once(t,"extern mono_mem;",
                            "extern unsigned char *mono_mem;",
                            "WiiMC FreeType mono font cache pointer")
    patch_file(root,"source/mplayer/libass/ass_font.c",subtitles_font_compat)

    # This 2010-era custom linker script omits exception symbols that
    # current libogc's interrupt handler requires. Use the *documented
    # standard address layout* from devkitPro/devkitppc-crtls:
    # libogc_common.ld, not a guessed RAM buffer.
    patch_file(root, "build/wiimc.ld",
               lambda t: replace_once(
                   t, "__isIPL = 0;",
                   "PROVIDE_HIDDEN(__ppc_excpt_buf = 0x800000d0);\n"
                   "PROVIDE_HIDDEN(__ppc_excpt_table = 0x80003000);\n"
                   "__isIPL = 0;",
                   "WiiMC modern libogc exception symbols"))
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
    def gui_exports(s):
        s = change(s)
        return replace_once(
            s, '#include "maizo_hid.inc"',
            '#include "maizo_hid.inc"\n'
            '/* One shared USB connection for PS1 emulation AND its GUI. */\n'
            'void maizo_gui_hid_poll(void){ maizo_hid_tick(); }\n'
            'unsigned int maizo_gui_hid_held(int port){ return maizo_hid_held(port); }',
            "WiiSXRX GUI HID exports")
    patch_file(root, "Gamecube/gc_input/controller-GC.c", gui_exports)
    # WiiSXRX uses PAD_ButtonsHeld() DIRECTLY in its separate GUI source;
    # the controller-GC.c emulator patch alone never reached its menus.
    def gui_focus(s):
        s = replace_once(s, '#include "FocusManager.h"',
                         '#include "FocusManager.h"\n'
                         'extern "C" unsigned int maizo_gui_hid_held(int port);',
                         "WiiSXRX GUI focus HID declarations")
        if s.count("PAD_ButtonsHeld(i)") != 2:
            raise ValueError("Unexpected WiiSXRX FocusManager PAD input layout")
        return s.replace("PAD_ButtonsHeld(i)",
                         "(PAD_ButtonsHeld(i) | maizo_gui_hid_held(i))")
    patch_file(root, "Gamecube/libgui/FocusManager.cpp", gui_focus)
    def gui_input(s):
        s = replace_once(s, '#include "InputManager.h"',
                         '#include "InputManager.h"\n'
                         'extern "C" void maizo_gui_hid_poll(void);\n'
                         'extern "C" unsigned int maizo_gui_hid_held(int port);',
                         "WiiSXRX GUI HID polling declarations")
        return replace_once(
            s, 'PAD_Clamp(gcPad);',
            'PAD_Clamp(gcPad);\n'
            '\tmaizo_gui_hid_poll();\n'
            '\tfor(int port=0;port<4;port++)\n'
            '\t\tgcPad[port].button |= maizo_gui_hid_held(port);',
            "WiiSXRX GUI HID frame polling")
    patch_file(root, "Gamecube/libgui/InputManager.cpp", gui_input)
    def gui_files(s):
        s = replace_once(s, '#include "FileBrowserFrame.h"',
                         '#include "FileBrowserFrame.h"\n'
                         'extern "C" unsigned int maizo_gui_hid_held(int port);',
                         "WiiSXRX GUI file browser HID declaration")
        return replace_once(
            s, 'u16 currentButtonsGC = PAD_ButtonsHeld(i);',
            'u16 currentButtonsGC = PAD_ButtonsHeld(i) | maizo_gui_hid_held(i);',
            "WiiSXRX file browser controller paging")
    patch_file(root, "Gamecube/menu/FileBrowserFrame.cpp", gui_files)
    # Upstream autoboot previously selected the LAST file in a directory
    # if the requested filename was absent (loop ended at num_entries-1).
    # That can start the wrong image and leave a black screen. Fail safely
    # to the real emulator menu instead.
    old_autoboot = """void fileBrowserFrame_AutoBootFile()
{
	int i;
	for(i = 0; i < num_entries - 1; i++)
		if(strcasestr(dir_entries[i].name, AutobootROM) != NULL)
			break;
	fileBrowserFrame_LoadFile(i);
}"""
    new_autoboot = """void fileBrowserFrame_AutoBootFile()
{
	if(dir_entries && num_entries > 0 && AutobootROM[0]){
		for(int i = 0; i < num_entries; i++){
			const char *name = filenameFromAbsPath(dir_entries[i].name);
			if(name && !(dir_entries[i].attr & FILE_BROWSER_ATTR_DIR) &&
			   strcasecmp(name, AutobootROM) == 0){
				fileBrowserFrame_LoadFile(i);
				return;
			}
		}
	}
	Autoboot = false;
	pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
	menu::MessageBox::getInstance().setMessage(
		"MAIZO: PS1 nao encontrado nesta pasta. Selecione o jogo pelo menu.");
}"""
    patch_file(root, "Gamecube/menu/FileBrowserFrame.cpp",
               lambda s: replace_once(s, old_autoboot, new_autoboot,
                                      "WiiSXRX exact autoboot filename"))
    # Preserve the exception-spec syntax used by this legacy WiiSXRX codebase.
    patch_file(root, "Gamecube/Makefile_Wii",
               lambda x: replace_once(
                   replace_once(x, "CFLAGS\t= -g -flto",
                                "CFLAGS\t= -fgnu89-inline -fcommon -g -flto",
                                "WiiSXRX legacy GCC C inline and common symbol semantics"),
                   "CXXFLAGS\t=\t$(CFLAGS)",
                   "CXXFLAGS\t=\t$(CFLAGS) -std=gnu++14", "WiiSXRX C++ standard"))
    patch_file(root, "Gamecube/Makefile_Wii",
               lambda x: replace_once(x, "$(DEVKITPPC)/bin/elf2dol",
                                      "elf2dol", "WiiSXRX current elf2dol location"))

    # Modern GCC no longer accepts implicit function prototypes and the
    # old BOOL typedef is not present in newer libogc headers.
    patch_file(root, "Gamecube/gc_input/controller.h",
               lambda x: replace_once(x, "BOOL          inUse;", "bool          inUse;", "WiiSXRX BOOL"))
    # The modern compiler does not treat an empty-parameter declaration as
    # a prototype, and CheckState(char *) clashes with legacy CheckState().
    # libogc provides static inline gettime/diff_usec in the watchdog header.
    # The old Peops FPS code only declared externs, causing linker failures.
    patch_file(root, "PeopsSoftGPU/fps.c",
               lambda x: replace_once(
                   replace_once(x, '#include "stdafx.h"',
                                '#include <ogc/lwp_watchdog.h>\n#include "stdafx.h"',
                                "WiiSXRX load watchdog before legacy BOOL macro"),
                   " long long gettime(void);\n"
                   " unsigned int diff_usec(long long start,long long end);",
                   " /* timers provided by libogc watchdog */",
                   "WiiSXRX remove obsolete timer prototypes"))
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
