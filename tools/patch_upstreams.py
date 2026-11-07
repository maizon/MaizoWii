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
GAME_VIDEO = SOURCE.parent / "maizo_game_video.inc"

def install_game_video(root: Path, folder: str):
    target=root / folder
    target.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(GAME_VIDEO,target / "maizo_game_video.inc")

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
    # WiiMC plays movies from USB mass storage. Do not enumerate, close or
    # initialize USB devices during playback. Copy HID input reports from
    # the existing async interrupt endpoint only; no USB IOCTL calls here.
    wiimc_hid = root / "source/maizo_hid.inc"
    wiimc_hid.write_text(wiimc_hid.read_text(encoding="utf-8") +
        "\nstatic void maizo_hid_tick_attached(void){\n"
        "  for(int i=0;i<4;i++){\n"
        "    mh_previous[i]=mh_buttons[i];\n"
        "    mh_buttons[i]=mh_live[i]?mh_report_buttons[i]:0;\n"
        "  }\n"
        "}\n",encoding="utf-8")
    # WiiMC settings are not a universal argv interface. Apply only the
    # numeric options that were validated against SWiiSettings after the
    # original settings.xml was loaded; leave credentials/text fields alone.
    shutil.copyfile(SOURCE.parent / "maizo_wiimc_settings.inc",
                    root / "source/maizo_wiimc_settings.inc")
    def wiimc_settings(t):
        t=replace_once(t, '#include "settings.h"',
            '#include "settings.h"\n#include "maizo_wiimc_settings.inc"',
            "WiiMC options bridge declaration")
        t=replace_once(t,'settingsLoaded = true; // attempted to load settings',
            'settingsLoaded = true; // attempted to load settings\n\tmaizo_wiimc_apply_profile();',
            "WiiMC apply Maizo options after XML load")
        return t
    patch_file(root, "source/settings.cpp", wiimc_settings)
    def wiimc_vi704(s):
        marker='GXRModeObj *vmode; // Menu video mode'
        insertion='''GXRModeObj *vmode; // Menu video mode
static bool maizo_vi704_active=false;
static void maizo_apply_video_width(void){
    if(!maizo_vi704_active || !vmode)return;
    vmode->viWidth=704;
    vmode->viXOrigin=8;
}
void MaizoForceVI704(int enabled){
    maizo_vi704_active=enabled==1;
    if(maizo_vi704_active&&vmode){
        maizo_apply_video_width();
        VIDEO_Configure(vmode);
        VIDEO_Flush();
    }
}
'''
        s=replace_once(s,marker,insertion,"WiiMC VI helper")
        # Internal mode transitions must retain the forced 704 VI window.
        s=s.replace('VIDEO_Configure(vmode);',
          'maizo_apply_video_width();\n\tVIDEO_Configure(vmode);')
        return s
    patch_file(root,"source/video.cpp",wiimc_vi704)
    # Libretro's old libogc/newlib runtime bridge; needed for the matched
    # precompiled WiiMC player to use memory, threads and FAT on GCC 2026.
    shutil.copyfile(SOURCE.parent / "wiimc_syscalls_compat.c",
                    root / "source/utils/maizo_syscalls_compat.c")
    def input_patch(s):
        s = replace_once(s,'#include "input.h"', '#include "input.h"\n#include "maizo_hid.inc"', "WiiMC")
        s = fix_pad_update(s, "PAD_ScanPads();", True)
        # On argv autoplay, the player may start before the GUI's
        # 180-frame HID warmup expires. Discover controllers once in
        # SetupPads AFTER the original WiiMC has initialized USB mass
        # storage; video playback only reads an attached HID snapshot.
        # Without this, autoplay could enter a permanent no-HID state.
        s = replace_once(
            s, "WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);",
            "WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);\n"
            "\t(void)maizo_hid_tick(); /* one-time discovery before autoplay */",
            "WiiMC argv-autoplay HID pre-attach")
        # Never dereference a missing WPAD_Data() with no Wiimote paired.
        # The original GuiTrigger embeds a zero-initialized WPADData.
        s = replace_once(s,
            "userInput[i].wpad = WPAD_Data(i);",
            "WPADData *connected=WPAD_Data(i);\n"
            "\t\tuserInput[i].wpad=connected?connected:&userInput[i].wpaddata;",
            "WiiMC no-Wiimote NULL guard")
        s = replace_once(s, "\tPAD_ScanPads();",
            "\tPAD_ScanPads();\n"
            "\tfor(int chan=0;chan<4;chan++){\n"
            "\t\tWPADData *connected=WPAD_Data(chan);\n"
            "\t\tif(!connected)memset(&userInput[chan].wpaddata,0,sizeof(WPADData));\n"
            "\t\tuserInput[chan].wpad=connected?connected:&userInput[chan].wpaddata;\n"
            "\t}",
            "WiiMC refresh live Wiimote pointers safely")
        # Avoid USB device enumeration while MPlayer reads USB storage.
        # An attached controller is asynchronously updated by the HID driver;
        # playback only snapshots its already-open input report.
        s = replace_once(s, "\tmaizo_hid_tick();",
            "\tstatic unsigned maizoHidWarmup=0;\n"
            "\tif(maizoHidWarmup<180)maizoHidWarmup++;\n"
            "\tif(maizoHidWarmup>=180){\n"
            "\t\tif(wiiIsPlaying())maizo_hid_tick_attached();\n"
            "\t\telse maizo_hid_tick();\n"
            "\t}",
            "WiiMC GUI-only HID USB enumeration")
        # DrawMPlayerGui calls UpdatePads immediately before MPlayerInput.
        # Read the same snapshot: calling PAD_ButtonsDown a second time can
        # lose HID edges and mismatch GUI/video controls.
        # Keep native WiiMC player input semantics. Only replace GC PAD
        # reads with the already-combined WiiMC userInput snapshots.
        # This preserves pause, seeking, OSD and volume commands unchanged.
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
    # Launch contract: support an explicit file from Maizo's argv[1] without
    # format-string interpretation. Preserve the upstream auto-play flow.
    def wiimc_argv(t):
        return replace_once(
            t, "sprintf(loadedFile, argv[1]);",
            'snprintf(loadedFile, sizeof(loadedFile), "%s", argv[1]);',
            "WiiMC bounded Maizo file argument")
    patch_file(root, "source/wiimc.cpp", wiimc_argv)
    # If the frontend passed a video and its FAT volume/partition is not
    # available after upstream mounted devices, return to the stock file
    # browser. Never continue to a permanently black autoplay screen.
    # Nothing changes for normal boot, HTTP streams or playable files.
    def media_preflight(t):
        return replace_once(t,
            "FindAppPath(); // Initialize SD and USB devices and look for apps/wiimc",
            """FindAppPath(); // Initialize SD and USB devices and look for apps/wiimc
    if(loadedFile[0] &&
       (!strncmp(loadedFile,"usb",3) || !strncmp(loadedFile,"sd",2))) {
        FILE *maizoMedia=fopen(loadedFile,"rb");
        if(!maizoMedia && !strncmp(loadedFile,"usb1:/",6)) {
            char requested[sizeof(loadedFile)];
            snprintf(requested,sizeof(requested),"%s",loadedFile);
            for(int v=2;v<=5 && !maizoMedia;v++){
                snprintf(loadedFile,sizeof(loadedFile),"usb%d:/%s",v,requested+6);
                maizoMedia=fopen(loadedFile,"rb");
            }
        }
        if(!maizoMedia){
            loadedFile[0]=0;
            AutobootExit=false;
            printf("MAIZO: missing media on mounted FAT, opening original browser\\n");
        } else fclose(maizoMedia);
    }""",
            "WiiMC missing argv media falls back to stock browser")
    patch_file(root,"source/wiimc.cpp",media_preflight)
    # WiiMC 3.0 file browser used sprintf(dst, "%s%s/", dst, name):
    # reading and writing the same buffer is undefined behavior, and happens
    # immediately when the user selects an SD/USB volume or folder.
    # Keep the stock browser, mount logic and UI; change ONLY the unsafe
    # append to bounded, non-overlapping strcat calls. Its preceding
    # MAXPATHLEN check guarantees room for the two appends.
    def wiimc_browser_guard(t):
        t=replace_once(t, 'sprintf(browser.dir, "%s%s/",browser.dir, browser.selIndex->file);',
            'strcat(browser.dir, browser.selIndex->file);\n'
            '\t\t\tstrcat(browser.dir, "/");',
            "WiiMC filesystem browser overlapping sprintf guard")
        t=replace_once(t,
            'static int UpdateDirName()\n{\n\tif(!browser.first || !browser.selIndex)',
            'static int UpdateDirName()\n{\n'
            '\t/* Invalid device entry: do not dereference while mounting FAT. */\n'
            '\tif(browser.selIndex && !browser.selIndex->file){\n'
            '\t\tbrowser.dir[0]=0;\n'
            '\t\tBrowserHistoryClear();\n'
            '\t\treturn -1;\n'
            '\t}\n'
            '\tif(!browser.first || !browser.selIndex)',
            "WiiMC browser null device entry guard")
        t=replace_once(t,
            'char *GetParentDir()\n{\n\tint size = strlen(browser.dir);',
            'char *GetParentDir()\n{\n\tint size = strlen(browser.dir);\n'
            '\tif(size==0){ parentdir[0]=0; return parentdir; }',
            "WiiMC browser empty-parent underflow guard")
        return t
    patch_file(root, "source/filebrowser.cpp", wiimc_browser_guard)
    # Keep stock WiiMC playback, video, GUI, autoplay and MPlayer threads
    # byte-for-byte original. Only controller translation is changed.
    # Earlier versions altered these paths and crashed with DSI on real Wii.
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
        # Preserve original _start with the matching 2010 WiiMC CRT0.
        s = replace_once(
            s, "KEEP (*crt0.o(*.init))",
            "KEEP (*(.crt0 .crt0.*))\n\t\tKEEP (*crt0.o(*.init))",
            "WiiMC modern crt0 section")
        # The bundled 2010 gu_psasm.o uses Unit01@sdarel(r13) as
        # paired-single load (offset requires 16-byte alignment).
        # Recent binutils no longer tolerate a misaligned SDA base.
        return replace_once(s, "\t.sdata     :\n\t{",
           "\t.sdata     :\n\t{\n"
           "\t\t. = ALIGN(16);\n"
           "\t\t_SDA_BASE_ = ADDR(.sdata) + 0x8000;\n"
           "\t\t/* This old gu_psasm.o has local Unit01 at section +4. */\n"
           "\t\t. = . + 12;\n"
           "\t\t*libogc.a:gu_psasm.o(.sdata)\n"
           "\t\t. = ALIGN(16);",
           "WiiMC bundled libogc paired-single SDA alignment")
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
        "/* Compatibility symbol for the old libmxml/newlib ABI. */\n"
        "static unsigned char maizo_ctype_table[257]={"+table+"};\n"
        "char *__ctype_ptr__=(char *)maizo_ctype_table;\n",
        encoding="utf-8")
    # The matching original WiiMC libogc already provides gettime,
    # DCFlushRange, DCStoreRangeNoSync and udelay. Never synthesize
    # aliases for these symbols: that gives multiple definitions and
    # can corrupt MPlayer/USB runtime boundaries.

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
    # Historical WiiMC GUI code and 2010-era portlibs rely on GCC
    # overflow/aliasing semantics that -O3 may invalidate on modern GCC.
    # Do not recompile the original FFmpeg/MPlayer archives at all.
    patch_file(root, "Makefile",
               lambda t: replace_once(
                   t, "CFLAGS\t\t=\t-g -O3 -Wall",
                   "CFLAGS\t\t=\t-g -O2 -fno-strict-aliasing -fwrapv -Wall",
                   "WiiMC conservative legacy GUI compiler semantics"))
    # Capture precise relocation/SDA placement from the final linker,
    # including failures; the link map is retained only inside CI.
    patch_file(root, "Makefile",
               lambda t: replace_once(
                 t, "-specs=wiimc.spec -Wl,-wrap,memcpy",
                 "-specs=wiimc.spec -Wl,-wrap,memcpy -Wl,-Map,/tmp/maizo_wiimc.map",
                 "WiiMC GNU ld link map diagnostic"))


    def fileop_compat(t):
        t = replace_once(t, "#include <string.h>",
                         "#include <string.h>\n#include <strings.h>\n#define stricmp strcasecmp",
                         "WiiMC portable stricmp")
        return replace_once(t, 'strstr(path, "../") > 0',
                            'strstr(path, "../") != NULL',
                            "WiiMC pointer comparison")
    patch_file(root, "source/fileop.cpp", fileop_compat)
    patch_file(root, "source/fileop.cpp",
        lambda t: replace_once(
            t, "if(!file || file[strlen(file)-1] == '/')",
            "if(!file || !*file || file[strlen(file)-1] == '/')",
            "WiiMC empty media filename GetExt guard"))

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
        # Keep the 2010 resetcallback() signature: the original WiiMC
        # libogc (restored by the WiiMC CI job) expects void(*)(void).
        # The modern variant changed it to void(*)(u32,void*), which
        # prevents compilation against the matching old SDK.
        return replace_once(t,
                         "static ssize_t __out_write(struct _reent *r, int fd,",
                         "static ssize_t __out_write(struct _reent *r, void *fd,",
                         "WiiMC newlib devoptab output callback")
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
        # Pinned WiiMC/libogc expects the original four-argument if_config.
        # Do not apply the five-argument modern libogc2 compatibility change.
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
                   "PROVIDE(__end__ = _end);\n"
                   "__isIPL = 0;",
                   "WiiMC modern libogc exception symbols"))
    patch_file(root, "source/mplayer/osdep/ave-rvl.c",
               lambda x: replace_once(x, "#include <gccore.h>",
                                      "#include <gccore.h>\n#include <string.h>\nextern void udelay(int us);",
                                      "WiiMC modern C compiler declarations"))


def patch_usbloader(root: Path):
    install_inc(root, "source")
    shutil.copyfile(SOURCE.parent / "maizo_wii_vi704.inc",
                    root / "source/patches/maizo_wii_vi704.inc")
    shutil.copyfile(SOURCE.parent / "maizo_usbloader_menu_vi704.inc",
                    root / "source/maizo_usbloader_menu_vi704.inc")
    def wii_menu_video(t):
        t=replace_once(t,'#include "gecko.h"',
          '#include "gecko.h"\n#include "maizo_usbloader_menu_vi704.inc"',
          "USB Loader GX VI menu helper")
        mark='static void ResetVideo_Menu()'
        start=t.index(mark)
        end=t.index('void InitVideo()',start)
        chunk=t[start:end]
        before='\tVIDEO_Configure(vmode);'
        if chunk.count(before)!=1:raise ValueError("USB GX menu VI call changed")
        chunk=chunk.replace(before,
            '\tif(maizo_loader_menu_vi704_requested()){\n'
            '\t\tvmode->viWidth=704;vmode->viXOrigin=8;\n'
            '\t}\n\tVIDEO_Configure(vmode);',1)
        return t[:start]+chunk+t[end:]
    patch_file(root,"source/video.cpp",wii_menu_video)
    def wii_game_modes(s):
        s=replace_once(s,
            'extern GXRModeObj *rmode;',
            'extern GXRModeObj *rmode;\n'
            'static void maizo_patch_game_vi704(u8 *dst,int len);\n'
            'static int maizo_wii_vi704_requested(void);',
            "USB Loader GX VI scan declaration")
        s=replace_once(s,'VideoModePatcher(dst, len, videoSelected, videoPatchDol);',
            'VideoModePatcher(dst, len, videoSelected, videoPatchDol);\n'
            '        if(maizo_wii_vi704_requested())maizo_patch_game_vi704(dst,len);',
            "USB Loader GX 704 game video patch")
        s=replace_once(s,'static bool compare_videomodes_rvl(GXRModeObj *mode1, GXRModeObjRVL *mode2)',
           '#include "maizo_wii_vi704.inc"\n'
           'static bool compare_videomodes_rvl(GXRModeObj *mode1, GXRModeObjRVL *mode2)',
           "Wii game VI matching helper")
        return s
    patch_file(root,"source/patches/gamepatches.c",wii_game_modes)
    def change(s):
        s = replace_once(s,'#include "input.h"', '#include "input.h"\n#include "maizo_hid.inc"', "USBLoader")
        s = fix_pad_update(s, "\tWPAD_ScanPads();\n\tPAD_ScanPads();", False)
        # A Wii without a paired Wiimote can return NULL from WPAD_Data.
        # Never pass NULL to memcpy: HID-only users must reach the GX menu.
        s = replace_once(s,
            "memcpy(&userInput[i].wpad, WPAD_Data(i), sizeof(WPADData));",
            "WPADData *maizo_remote = WPAD_Data(i);\n"
            "\t\tif(maizo_remote)memcpy(&userInput[i].wpad, maizo_remote, sizeof(WPADData));\n"
            "\t\telse memset(&userInput[i].wpad, 0, sizeof(WPADData));",
            "USBLoader no-Wiimote input guard")

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
    # Game screen has a separate GX quad from its menu. Do not change menu VI.
    install_game_video(root,"PeopsSoftGPU")
    def video_game(s):
        s=replace_once(s,'#include "draw.h"',
           '#include "draw.h"\n#include "maizo_game_video.inc"',
           "WiiSXRX gameplay-only video calibration")
        s=replace_once(s,'float xcoord = 1.0;',
           'MaizoGameVideo cal=maizo_game_video(maizo_game_native_output());\n'
           '\tfloat maizo_x=cal.x/320.0f,maizo_y=cal.y/240.0f;\n'
           '\tfloat xcoord = cal.w/100.0f;',
           "WiiSXRX game width")
        s=replace_once(s,'float ycoord = 1.0;',
           'float ycoord = cal.h/100.0f;',
           "WiiSXRX game height")
        for old,new in (
          ('GX_Position2f32(-xcoord, ycoord);','GX_Position2f32(-xcoord+maizo_x, ycoord+maizo_y);'),
          ('GX_Position2f32( xcoord, ycoord);','GX_Position2f32( xcoord+maizo_x, ycoord+maizo_y);'),
          ('GX_Position2f32( xcoord,-ycoord);','GX_Position2f32( xcoord+maizo_x,-ycoord+maizo_y);'),
          ('GX_Position2f32(-xcoord,-ycoord);','GX_Position2f32(-xcoord+maizo_x,-ycoord+maizo_y);')):
            s=replace_once(s,old,new,"WiiSXRX GX gameplay quad")
        return s
    patch_file(root,"PeopsSoftGPU/drawGX.c",video_game)
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



def patch_wiisx422(root: Path):
    """Beta 4.2.2: the stock archive has no Maizo HID layer at all.
    Inject the same proven-in-source GC/GUI bridge as WiiSXRX, but use
    the 4.2.2 CMake source layout. Hardware testing remains required.
    """
    install_inc(root, "Gamecube/gc_input")
    def core(t):
        t = replace_once(t, '#include "controller.h"',
            '#include "controller.h"\n#include "maizo_hid.inc"\n'
            'void maizo_gui_hid_poll(void){ maizo_hid_tick(); }\n'
            'unsigned int maizo_gui_hid_held(int port){ return maizo_hid_held(port); }',
            "WiiSX 4.2.2 native HID bridge")
        t = replace_once(t, 'unsigned int b = PAD_ButtonsHeld(Control);',
            'unsigned int b = PAD_ButtonsHeld(Control) | maizo_hid_held(Control);',
            "WiiSX 4.2.2 held buttons")
        scan='if(padNeedScan){ gc_connected = PAD_ScanPads(); padNeedScan = 0; }'
        if t.count(scan)!=2:
            raise ValueError("WiiSX 4.2.2 scan sites changed")
        t=t.replace(scan,scan+'\n\tif(maizo_hid_connected(0)) gc_connected |= 1;')
        for native,shim in (("PAD_StickX","maizo_hid_stickX"),
                            ("PAD_StickY","maizo_hid_stickY"),
                            ("PAD_SubStickX","maizo_hid_substickX"),
                            ("PAD_SubStickY","maizo_hid_substickY")):
            t=t.replace(native+"(Control)",shim+"(Control, "+native+"(Control))")
        # In-game input may tick independent of UI frame polling.
        t=replace_once(t, "static int _GetKeys(int Control, BUTTONS * Keys, controller_config_t* config, int psxType)\n{",
            "static int _GetKeys(int Control, BUTTONS * Keys, controller_config_t* config, int psxType)\n{\n\tmaizo_hid_tick();",
            "WiiSX 4.2.2 in-game HID polling")
        return t
    patch_file(root, "Gamecube/gc_input/controller-GC.c", core)
    def input_mgr(t):
        t=replace_once(t, '#include "InputManager.h"',
            '#include "InputManager.h"\n'
            'extern "C" void maizo_gui_hid_poll(void);\n'
            'extern "C" unsigned int maizo_gui_hid_held(int port);',
            "WiiSX 4.2.2 GUI HID exports")
        return replace_once(t, 'PAD_Clamp(gcPad);',
            'PAD_Clamp(gcPad);\n'
            '\tmaizo_gui_hid_poll();\n'
            '\tfor(int port=0;port<4;port++)\n'
            '\t\tgcPad[port].button |= maizo_gui_hid_held(port);\n'
            '\tif(maizo_gui_hid_held(0))gc_connected |= 1;',
            "WiiSX 4.2.2 GUI frame HID polling")
    patch_file(root, "Gamecube/libgui/InputManager.cpp", input_mgr)
    def focus(t):
        t=replace_once(t, '#include "FocusManager.h"',
            '#include "FocusManager.h"\n'
            'extern "C" unsigned int maizo_gui_hid_held(int port);',
            "WiiSX 4.2.2 GUI focus HID bridge")
        if t.count("PAD_ButtonsHeld(i)")!=2:
            raise ValueError("WiiSX 4.2.2 FocusManager changed")
        return t.replace("PAD_ButtonsHeld(i)",
            "(PAD_ButtonsHeld(i) | maizo_gui_hid_held(i))")
    patch_file(root, "Gamecube/libgui/FocusManager.cpp", focus)
    def browser(t):
        t=replace_once(t, '#include "FileBrowserFrame.h"',
            '#include "FileBrowserFrame.h"\n'
            'extern "C" unsigned int maizo_gui_hid_held(int port);',
            "WiiSX 4.2.2 file picker HID bridge")
        return replace_once(t, 'u16 currentButtonsGC = PAD_ButtonsHeld(i);',
            'u16 currentButtonsGC = PAD_ButtonsHeld(i) | maizo_gui_hid_held(i);',
            "WiiSX 4.2.2 file picker HID page controls")
    patch_file(root,"Gamecube/menu/FileBrowserFrame.cpp",browser)
    # WiiSX beta 4.2.2 uses game-only VI entrypoints. Menus are unchanged.
    install_game_video(root,"Gamecube/libgui")
    def game_video(s):
        s=replace_once(s,'#include "GraphicsGX.h"',
          '#include "GraphicsGX.h"\n#include "maizo_game_video.inc"',
          "WiiSX 4.2.2 game-only video profile")
        s=replace_once(s,'vmode->viWidth = 640;\n\tvmode->viXOrigin = (VI_MAX_WIDTH_PAL - vmode->viWidth) / 2;\n\tVIDEO_Init ();',
          'vmode->viWidth = 640;\n\tvmode->viXOrigin = (VI_MAX_WIDTH_PAL - vmode->viWidth) / 2;\n'
          '\tmaizo_video_adjust_vi(vmode,0);\n\tVIDEO_Init ();',
          "WiiSX 4.2.2 interlaced game VI geometry")
        s=replace_once(s,'if(is_pal) {\n\t\tm = &TVPal264Ds;\n\t}',
          'if(is_pal) {\n\t\tm = &TVPal264Ds;\n\t}\n'
          '    static GXRModeObj maizo_native_video;\n'
          '    memcpy(&maizo_native_video,m,sizeof(maizo_native_video));\n'
          '    m=&maizo_native_video;\n'
          '    maizo_video_adjust_vi(m,1);',
          "WiiSX 4.2.2 progressive game VI geometry")
        return s
    patch_file(root,"Gamecube/libgui/GraphicsGX.cpp",game_video)


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

def patch_gx_vi704(root: Path):
    """Optional 704 VI window for GX menus and game renderers.
    Preserve fbWidth, vertical timing and 240p line count.
    """
    install_game_video(root, "source/drivers/ogc")
    def driver(s):
        s=replace_once(s,'#include "OgcVideoDriver.h"',
            '#include "OgcVideoDriver.h"\n#include "maizo_game_video.inc"',
            "GX menu VI 704 include")
        a=s.index('GXRModeObj* OgcVideoDriver::findVideoMode()')
        b=s.index('void OgcVideoDriver::startMenuVideo()',a)
        original=s[a:b]
        probe='\treturn mode;'
        if original.count(probe)!=1:
            raise ValueError("GX findVideoMode final return not found exactly once")
        revised=original.replace(probe,
            '\tif(maizo_game_force_vi704())maizo_video_adjust_vi(mode,0);\n'
            '\treturn mode;',1)
        return s[:a]+revised+s[b:]
    patch_file(root,"source/drivers/ogc/OgcVideoDriver.cpp",driver)

def patch_gx(root: Path):
    patch_gx_vi704(root)
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
    def nes_gba_video(s):
        s=replace_once(s,'#include "OgcEmulatorVideo.h"',
             '#include "OgcEmulatorVideo.h"\n#include "maizo_game_video.inc"',
             "NES/GBA gameplay VI")
        s=replace_once(s,'videoDriver->setupVideoMode(rmode); // reconfigure VI',
            'static GXRModeObj maizo_game_vi;\n'
            '\tmemcpy(&maizo_game_vi,rmode,sizeof(maizo_game_vi));\n'
            '\trmode=&maizo_game_vi;\n'
            '\tif(maizo_game_force_vi704())\n'
            '\t\tmaizo_video_adjust_vi(rmode,rmode->xfbHeight<=288);\n'
            '\tvideoDriver->setupVideoMode(rmode); // reconfigure VI',
            "NES/GBA game VI 704")
        return s
    patch_file(root,"source/drivers/ogc/OgcEmulatorVideo.cpp",nes_gba_video)

def patch_snes(root: Path):
    patch_gx_vi704(root)
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
    # SNES game quad is built independently from GUI; adapt the quad only.
    install_game_video(root,"source/drivers/ogc")
    def game_video(s):
        s=replace_once(s,'#include "OgcEmulatorVideo.h"',
          '#include "OgcEmulatorVideo.h"\n#include "maizo_game_video.inc"',
          "SNES game-only video profile")
        # The original 240p path uses a 512px EFB and a VI window of
        # ~644 physical samples. Moving its GX quad cannot correct
        # asymmetric CRT border/overscan. Adjust the *VI* only for 240p;
        # retain the upstream pixel/line geometry and 480i quad controls.
        s=replace_once(s, 'videoDriver->setupVideoMode(rmode); // reconfigure VI',
          'static GXRModeObj maizo_game_vi;\n'
          '\tmemcpy(&maizo_game_vi,rmode,sizeof(maizo_game_vi));\n'
          '\trmode=&maizo_game_vi;\n'
          '\tif(EmuSettings.videoMode == VIDEOMODE_ORIGINAL_240P)\n'
          '\t\tmaizo_video_adjust_vi(rmode,1);\n'
          '\telse if(maizo_game_force_vi704())\n'
          '\t\tmaizo_video_adjust_vi(rmode,0);\n'
          '\tvideoDriver->setupVideoMode(rmode); // reconfigure VI',
          "SNES centered hardware VI for 240p")
        s=replace_once(s,'xscale *= EmuSettings.videoZoomHor;',
          'MaizoGameVideo cal=maizo_game_video(EmuSettings.videoMode == VIDEOMODE_ORIGINAL_240P);\n'
          '\t\tif(EmuSettings.videoMode != VIDEOMODE_ORIGINAL_240P)\n'
          '\t\t\txscale = xscale * (int)cal.w / 100;\n'
          '\t\tyscale = yscale * (int)cal.h / 100;\n'
          '\t\tint maizo_x=EmuSettings.videoMode == VIDEOMODE_ORIGINAL_240P?0:cal.x * (int)vmode->fbWidth / 640;\n'
          '\t\tint maizo_y=cal.y * (int)vmode->efbHeight / 480;\n'
          '\t\txscale *= EmuSettings.videoZoomHor;',
          "SNES game geometry video mode")
        s=replace_once(s,'square[6] = square[3]  =  xscale + EmuSettings.videoXshift;',
          'square[6] = square[3]  =  xscale + EmuSettings.videoXshift + maizo_x;',
          "SNES right game quad")
        s=replace_once(s,'-xscale + EmuSettings.videoXshift;',
          '-xscale + EmuSettings.videoXshift + maizo_x;',
          "SNES left game quad")
        s=replace_once(s,'square[4] = square[1]  =  yscale - EmuSettings.videoYshift;',
          'square[4] = square[1]  =  yscale - EmuSettings.videoYshift + maizo_y;',
          "SNES upper game quad")
        s=replace_once(s,'-yscale - EmuSettings.videoYshift;',
          '-yscale - EmuSettings.videoYshift + maizo_y;',
          "SNES bottom game quad")
        return s
    patch_file(root,"source/drivers/ogc/OgcEmulatorVideo.cpp",game_video)

MODULES = {
  "WiiMC": patch_wiimc,
  "usbloader_gx": patch_usbloader,
  "WiiSXRX": patch_wiisx,
  "WiiSX": patch_wiisx422,
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
