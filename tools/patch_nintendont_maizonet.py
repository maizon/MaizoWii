#!/usr/bin/env python3
"""Patch pinned FIX94/Nintendont for MaizoNet experiments.

Keep Nintendont kernel/build isolated from the MaizoWii frontend.
The kernel SMB1/ISO path remains experimental and UNVALIDATED on Wii hardware.
Never package as production before physical GameCube gameplay tests.
"""
from pathlib import Path
import shutil
import sys

def replace_one(path: Path, before: str, after: str) -> None:
    s=path.read_text(encoding="utf-8")
    if s.count(before)!=1:
        raise RuntimeError(f"{path}: expected exactly one matching anchor, got {s.count(before)}")
    path.write_text(s.replace(before,after),encoding="utf-8")


def patch_nin_kernel(root: Path) -> None:
    """Insert isolated read-only SMB1 ISO backend without changing FAT path."""
    src=Path(__file__).resolve().parents[1] / "integrations"
    kernel=root / "kernel"

    # The pinned Nintendont loader omits CTRL_SYNC from disk_ioctl().
    # FatFs f_sync() therefore fails even after f_write() succeeds.
    # Properly flush the libfat disk cache and propagate genuine IO errors.
    diskio = root / "loader" / "source" / "diskio.c"
    replace_one(
        diskio,
        "\t\tcase GET_SECTOR_COUNT:\n\t\t\t// TODO?\n\t\t\tbreak;",
        "\t\tcase GET_SECTOR_COUNT:\n\t\t\t// TODO?\n\t\t\tbreak;\n"
        "\n\t\tcase CTRL_SYNC:\n"
        "\t\t\tret = cache[pdrv] && _FAT_cache_flush(cache[pdrv])\n"
        "\t\t\t    ? RES_OK : RES_ERROR;\n"
        "\t\t\tbreak;"
    )
    for name in ("maizonet_smb1.h","maizonet_smb1.c",
                 "maizonet_nintendont_net.h","maizonet_nintendont_net.c",
                 "maizonet_net_ipv4.h", "maizonet_udp.h", "maizonet_udp.c"):
        shutil.copyfile(src / name,kernel / name)

    replace_one(
        kernel / "Makefile",
        "SDI.o usb.o usbstorage.o wdvd.o sock.o",
        "SDI.o usb.o usbstorage.o wdvd.o sock.o maizonet_smb1.o maizonet_udp.o maizonet_nintendont_net.o"
    )
    iso=kernel / "ISO.c"
    replace_one(iso,
       '#include "wdvd.h"',
       '#include "wdvd.h"\n#include "maizonet_nintendont_net.h"')
    replace_one(iso,
       "u32 ISOFileOpen = 0;",
       "u32 ISOFileOpen = 0;\nstatic bool mznet_iso = false;")
    replace_one(iso,
       "\tif(ISOFileOpen == 0)\n\t\treturn;\n\n\tUINT read;",
       "\tif(ISOFileOpen == 0)\n\t\treturn;\n"
       "\t/* A remote ISO uses the same disc-sector demand path as FAT. */\n"
       "\tif(mznet_iso)\n\t{\n"
       "\t\tif(!maizonet_nin_read(Buffer,Length,Offset64))\n"
       "\t\t{\n\t\t\tdbgprintf(\"MZNET: remote sector timeout\\r\\n\");\n"
       "\t\t\tmaizonet_nin_fatal(\"ISO_READ\",Offset64,Length);\n\t\t}\n"
       "\t\tLastOffset64=Offset64+Length;\n"
       "\t\tUSBReadTimer=read32(HW_TIMER);\n"
       "\t\treturn;\n\t}\n\n\tUINT read;")
    replace_one(iso,
       "\telse\n\t{\n\t\ts32 ret = f_open_char( &GameFile, ConfigGetGamePath(), FA_READ|FA_OPEN_EXISTING );",
       "\telse if(maizonet_nin_requested(ConfigGetCheatPath()))\n\t{\n"
       "\t\tif(!maizonet_nin_open(ConfigGetCheatPath()))return false;\n"
       "\t\tmznet_iso=true;\n\t}\n"
       "\telse\n\t{\n\t\ts32 ret = f_open_char( &GameFile, ConfigGetGamePath(), FA_READ|FA_OPEN_EXISTING );")
    replace_one(iso,
       "\tif (tmp_ciso->magic == CISO_MAGIC)\n\t{",
       "\tif (tmp_ciso->magic == CISO_MAGIC)\n\t{\n"
       "\t\t/* Only raw ISO/GCM over SMB1; no CISO remapping yet. */\n"
       "\t\tif(mznet_iso)\n\t\t{\n"
       "\t\t\tfree(tmp_ciso);maizonet_nin_close();\n"
       "\t\t\tmznet_iso=false;ISOFileOpen=0;return false;\n\t\t}")
    # ISOSeek() can be called independently of ISOReadDirect(). A remote
    # image has no live FAT FIL, so never f_lseek() that uninitialized object.
    replace_one(iso,
       "\tif(LastOffset64 != Offset64)\n\t{\n\t\tif (ISO_IsCISO)",
       "\tif(LastOffset64 != Offset64)\n\t{\n"
       "\t\tif(mznet_iso)\n\t\t{\n"
       "\t\t\tLastOffset64 = Offset64;\n\t\t\treturn;\n\t\t}\n"
       "\t\tif (ISO_IsCISO)")
    replace_one(iso,
       "ISOReadDirect(tmp_ciso, 0x8000, 0);",
       "ISOReadDirect(tmp_ciso, 0x8000, 0);\n"
       "\tif(mznet_iso && maizonet_nin_failed())\n"
       "\t{\n"
       "\t\tfree(tmp_ciso);maizonet_nin_close();\n"
       "\t\tmznet_iso=false;ISOFileOpen=0;return false;\n"
       "\t}")
    # OPL reads requested CDVD sectors in bounded chunks rather than
    # speculatively doubling tiny reads up to 64KiB. Use the same policy
    # for remote ISO; retain native Nintendont cache and local behavior.
    replace_one(iso,
       "if( (Offset64 == LastOffset64) && (*Length < 0x8000) )",
       "if( !mznet_iso && (Offset64 == LastOffset64) && (*Length < 0x8000) )")
    replace_one(iso,
       "void ISOClose()\n{\n\tif(ISOFileOpen)\n\t{\n\t\tif(wiiVCInternal)",
       "void ISOClose()\n{\n\tif(ISOFileOpen)\n\t{\n"
       "\t\tif(mznet_iso)maizonet_nin_close();\n"
       "\t\telse if(wiiVCInternal)")
    replace_one(iso,
       "\tISOFileOpen = 0;\n\tISO_IsCISO = false;\n}",
       "\tISOFileOpen = 0;\n\tISO_IsCISO = false;\n\tmznet_iso=false;\n}")
    # The frontend's network worked, but the handoff to Nintendont's IOS
    # returned KD=-33 and SOGetHostID=0 on real Wii. Initialise via libogc
    # on the PPC *before* the loader's first disc-header ioctl reaches the
    # ARM kernel. Stock/local games never perform this extra step.
    loader=root / "loader" / "source" / "main.c"
    replace_one(loader,
        '#include <unistd.h>',
        '#include <unistd.h>\n#include <network.h>')
    # The on-screen -9 is MaizoNet ARM's explicit fatal status, NOT a
    # generic storage error. Show its STATUS_ERROR code without requiring
    # a diagnostic.log file to be written on the Wii FAT volume.
    replace_one(
        loader,
        'PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*6, "Loading patched kernel... %d", STATUS_LOADING);',
        'PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*6, "Loading patched kernel... %d", STATUS_LOADING);\n'
        '\t\t\tif(STATUS_LOADING == -9)\n'
        '\t\t\t{\n'
        '\t\t\t\tPrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7,\n'
        '\t\t\t\t\t"MAIZONET ARM ERROR: CODE=%d", (s32)STATUS_ERROR);\n'
        '\t\t\t\tPrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*8,\n'
        '\t\t\t\t\t"138=SOCKET 120=SMB 121=OPEN 130=READ");\n'
        '\t\t\t}'
    )

    # Before the FIRST remote ISO header read, select the same valid NCD
    # profile that Wii settings already have enabled. Nintendont's original
    # profile activation runs much later, after the first DI reads, and only
    # for BBA-emulation games. Remote ISO needs it much earlier. Do not set
    # NIN_CFG_BBA_EMU: XIII and ordinary GameCube games must not see a BBA.
    replace_one(loader,
        'static ioctlv IOCTL_Buf[2] ALIGNED(32);',
        'static ioctlv IOCTL_Buf[2] ALIGNED(32);\n'
        'static int MaizoNetEnableIOSNetworkProfile(void)\n'
        '{\n'
        '\tu8 *cfg = memalign(32,0x1C00);\n'
        '\tif(!cfg)return -12;\n'
        '\tmemset(cfg,0,0x1C00);\n'
        '\ts32 fd = IOS_Open("/dev/net/ncd/manage",0);\n'
        '\tif(fd<0){free(cfg);return fd;}\n'
        '\tioctlv v[2] ALIGNED(32);\n'
        '\tv[0].data = cfg; v[0].len = 0x1B5C;\n'
        '\tv[1].data = cfg+0x1B60; v[1].len = 0x20;\n'
        '\ts32 ret=IOS_Ioctlv(fd,3,0,2,v);\n'
        '\tif(ret>=0)\n'
        '\t{\n'
        '\t\tconst unsigned ofs[3]={8u,0x924u,0x1240u};\n'
        '\t\tunsigned chosen=3,i;\n'
        '\t\t/* Prefer the active Wii profile; otherwise an enabled one. */\n'
        '\t\tfor(i=0;i<3;i++)if((cfg[ofs[i]]&0xA0)==0xA0){chosen=i;break;}\n'
        '\t\tif(chosen==3)for(i=0;i<3;i++)if(cfg[ofs[i]]&0x80){chosen=i;break;}\n'
        '\t\tif(chosen<3)\n'
        '\t\t{\n'
        '\t\t\tfor(i=0;i<3;i++)\n'
        '\t\t\t{\n'
        '\t\t\t\tif(i==chosen)cfg[ofs[i]] |= 0xA0;\n'
        '\t\t\t\telse cfg[ofs[i]] &= 0x7F;\n'
        '\t\t\t}\n'
        '\t\t\tcfg[4] = (cfg[ofs[chosen]]&1) ? 2 : 1;\n'
        '\t\t\tDCFlushRange(cfg,0x1C00);\n'
        '\t\t\tret=IOS_Ioctlv(fd,4,1,1,v);\n'
        '\t\t}\n'
        '\t\telse ret=-2; /* No configured profile: do not invent one. */\n'
        '\t}\n'
        '\tIOS_Close(fd);free(cfg);return ret;\n'
        '}\n')
    replace_one(loader,
        '\t// Get multi-game and region code information.',
        '\tif(strncmp(ncfg->CheatPath,"MZSMB1|",7)==0 || strncmp(ncfg->CheatPath,"MZUDP1|",7)==0)\n'
        '\t{\n'
        '\t\t/* libogc net_init: NCD link, NWC24 startup, SO, DHCP. */\n'
        '\t\tint net_status = net_init();\n'
        '\t\tint profile_status = 0;\n'
        '\t\t/* Do not reconfigure NCD when the Wii network is healthy. */\n'
        '\t\tif(net_status<0)\n'
        '\t\t{\n'
        '\t\t\tprofile_status = MaizoNetEnableIOSNetworkProfile();\n'
        '\t\t\tif(profile_status>=0)net_status=net_init();\n'
        '\t\t}\n'
        '\t\tgprintf("MZNET: Wii NCD fallback=%d / PPC network=%d\\r\\n",profile_status,net_status);\n'
        '\t\t/* Keep an independent diagnostic even if ARM never starts. */\n'
        '\t\tFIL logfile;\n'
        '\t\tif(f_open_char(&logfile,"/maizo/maizonet/diagnostic.log",\n'
        '\t\t               FA_OPEN_ALWAYS|FA_WRITE)==FR_OK)\n'
        '\t\t{\n'
        '\t\t\tchar logline[96]; UINT logged=0;\n'
        '\t\t\tsnprintf(logline,sizeof(logline),"MZNET_LOADER_NET_INIT result=%d\\n",net_status);\n'
        '\t\t\tf_lseek(&logfile,f_size(&logfile));\n'
        '\t\t\tf_write(&logfile,logline,strlen(logline),&logged);\n'
        '\t\t\tf_sync(&logfile); f_close(&logfile);\n'
        '\t\t}\n'
        '\t}\n'
        '\t// Get multi-game and region code information.')

    # Stock Nintendont intentionally suspends the WiiConnect24 scheduler
    # just before handing off to the GameCube game. That is a sensible local
    # disc default but interferes with a network-backed virtual disc which
    # MUST keep its IOS network services alive while gameplay is running.
    # This exception applies only to the MaizoNet transport marker.
    replace_one(loader,
        '\tfd = IOS_Open("/dev/net/kd/request", 0);\n'
        '\tIOS_Ioctl(fd, IOCTL_ExecSuspendScheduler, NULL, 0, &out, 4);\n'
        '\tIOS_Close(fd);',
        '\tif(strncmp(ncfg->CheatPath,"MZSMB1|",7)!=0 && strncmp(ncfg->CheatPath,"MZUDP1|",7)!=0)\n'
        '\t{\n'
        '\t\tfd = IOS_Open("/dev/net/kd/request", 0);\n'
        '\t\tif(fd>=0)\n'
        '\t\t{\n'
        '\t\t\tIOS_Ioctl(fd, IOCTL_ExecSuspendScheduler, NULL, 0, &out, 4);\n'
        '\t\t\tIOS_Close(fd);\n'
        '\t\t}\n'
        '\t}\n'
        '\t/* Remote ISO must retain NWC24 service for the Nintendont ARM SMB1 socket. */')

    # The Wii currently shows a completely blank screen because the
    # ARM /dev/mydi reader can fail before game handoff without giving
    # the PPC loader a usable answer. Require one read *past* the local
    # 128 KiB stub while the loader still has video/UI. This tests real
    # SMB-backed ARM sector transport rather than merely PPC net_init().
    replace_one(loader,
        '\tif(argsboot == false)\n\t{\n\t\tDrawBuffer(); // Draw all status messages',
        '\tif(strncmp(ncfg->CheatPath,"MZSMB1|",7)==0 || strncmp(ncfg->CheatPath,"MZUDP1|",7)==0)\n'
        '\t{\n'
        '\t\tShowMessageScreen("MAIZONET: TESTING ARM NETWORK DISC READER...");\n'
        '\t\tint net_di=IOS_Open("/dev/mydi",0);\n'
        '\t\tif(net_di<0)ShowMessageScreenAndExit("MaizoNet: ARM network ISO open FAILED",1);\n'
        '\t\tu8 *test=memalign(32,32);\n'
        '\t\tif(!test)ShowMessageScreenAndExit("MaizoNet: ISO check memory FAILED",1);\n'
        '\t\tmemset(test,0xA5,32);DCFlushRange(test,32);\n'
        '\t\tint ret=IOS_Ioctl(net_di,3,(void*)0x20000,0,test,32);\n'
        '\t\tDCInvalidateRange(test,32);\n'
        '\t\tIOS_Close(net_di);free(test);\n'
        '\t\tif(ret<0)ShowMessageScreenAndExit("MaizoNet: ARM network sector read FAILED",1);\n'
        '\t\tShowMessageScreen("MAIZONET: ARM ISO SECTOR READ OK");\n'
        '\t}\n'
        '\tif(argsboot == false)\n'
        '\t{\n'
        '\t\tDrawBuffer(); // Draw all status messages')

    # A partial 128 KiB stub must NEVER be retried through FST then shut down.
    di=kernel / "DI.c"
    replace_one(di,
        '#include "ISO.h"',
        '#include "ISO.h"\n#include "maizonet_nintendont_net.h"')
    replace_one(di,
        '\t\t\t\t\tif( ISOInit() == false )\n\t\t\t\t\t{\n'
        '\t\t\t\t\t\t_sprintf( GamePath, "%s", ConfigGetGamePath() );',
        '\t\t\t\t\tif( ISOInit() == false )\n\t\t\t\t\t{\n'
        '\t\t\t\t\t\tif(maizonet_nin_requested(ConfigGetCheatPath()))\n'
        '\t\t\t\t\t\t{\n'
        '\t\t\t\t\t\t\tif(!maizonet_nin_failed())maizonet_nin_fatal("ISO_OPEN",0,0);\n'
        '\t\t\t\t\t\t\tmqueue_ack(di_msg, -70);\n'
        '\t\t\t\t\t\t\tbreak; /* Never silently fall back to 128-KiB stub. */\n'
        '\t\t\t\t\t\t}\n'
        '\t\t\t\t\t\t_sprintf( GamePath, "%s", ConfigGetGamePath() );')

    # If an ISO sector read fails, ACK the PPC ioctl with an error rather
    # than copying invalid bytes or leaving PPC blocked on an IPC reply.
    replace_one(di,
        '\t\t\t\t\tif(((u32)di_dest + Offset) <= 0x31A0)',
        '\t\t\t\t\t\tif(maizonet_nin_requested(ConfigGetCheatPath()) && maizonet_nin_failed())\n'
        '\t\t\t\t\t\t\tbreak; /* Abort partial remote read. */\n'
        '\t\t\t\t\t\tif(((u32)di_dest + Offset) <= 0x31A0)')
    replace_one(di,
        '\t\t\t\tif(di_msg->ioctl.command == 0)',
        '\t\t\t\tif(maizonet_nin_requested(ConfigGetCheatPath()) && maizonet_nin_failed())\n'
        '\t\t\t\t{\n'
        '\t\t\t\t\tmqueue_ack(di_msg, -70);\n'
        '\t\t\t\t\tbreak;\n'
        '\t\t\t\t}\n'
        '\t\t\t\tif(di_msg->ioctl.command == 0)')

    # Direct executable: patch the pinned Nintendont loader itself. Both
    # independent launcher and ARM kernel use the same bounded SMB1 protocol,
    # but separate compilation units/addresses. No MaizoWii argv dependency.
    loaderdir = root / "loader" / "source"
    for name in ("maizonet_loader.c", "maizonet_loader.h",
                 "maizonet_smb1.c", "maizonet_smb1.h"):
        shutil.copyfile(src / name, loaderdir / name)
    menu = loaderdir / "menu.c"
    replace_one(menu, '#include "menu.h"',
                '#include "menu.h"\n#include "maizonet_loader.h"')
    replace_one(menu,
        '\tbool SaveSettings = false;\n\tbool redraw = true;\t// Need to draw the menu the first time.',
        '\tbool SaveSettings = false;\n'
        '\tint maizo_selection=(ncfg->Config & NIN_CFG_USB)?1:0;\n'
        '\tbool redraw = true;\t// Need to draw the menu the first time.')
    replace_one(menu,
        'MENU_POS_Y + 20 * 6, UseSD ? ARROW_LEFT : "");',
        'MENU_POS_Y + 20 * 6, maizo_selection==0 ? ARROW_LEFT : "");')
    replace_one(menu,
        'MENU_POS_Y + 20 * 7, UseSD ? "" : ARROW_LEFT);',
        'MENU_POS_Y + 20 * 7, maizo_selection==1 ? ARROW_LEFT : "");')
    replace_one(menu,
        'MENU_POS_Y + 20 * 7, "USB  ");',
        'MENU_POS_Y + 20 * 7, "USB  ");\n'
        '\t\t\tPrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 8, '
        'maizo_selection==2 ? ARROW_LEFT : "");\n'
        '\t\t\tPrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 8, '
        '"SMB  ");')
    replace_one(menu,
        '\t\tif (FPAD_OK(0))\n'
        '\t\t{\n'
        '\t\t\t// Select a game from the specified device.\n'
        '\t\t\tint ret = SelectGame();\n'
        '\t\t\tif (ret & 2) SaveSettings = true;\n'
        '\t\t\tif (ret & 1) break;\n'
        '\t\t\tredraw = true;\n'
        '\t\t}\n'
        '\t\telse if (FPAD_Start(0))\n'
        '\t\t{\n'
        '\t\t\tShowMessageScreenAndExit("Returning to loader...", 0);\n'
        '\t\t}\n'
        '\t\telse if (FPAD_Down(0))\n'
        '\t\t{\n'
        '\t\t\tncfg->Config = ncfg->Config | NIN_CFG_USB;\n'
        '\t\t\tredraw = true;\n'
        '\t\t}\n'
        '\t\telse if (FPAD_Up(0))\n'
        '\t\t{\n'
        '\t\t\tncfg->Config = ncfg->Config & ~NIN_CFG_USB;\n'
        '\t\t\tredraw = true;\n'
        '\t\t}',
        '\t\tif (FPAD_OK(0))\n'
        '\t\t{\n'
        '\t\t\tif(maizo_selection==2)\n'
        '\t\t\t{\n'
        '\t\t\t\tif(MaizoNetSelectGame())break;\n'
        '\t\t\t}\n'
        '\t\t\telse\n'
        '\t\t\t{\n'
        '\t\t\t\t/* Never carry a stale remote marker into local games. */\n'
        '\t\t\t\tif(strncmp(ncfg->CheatPath,"MZSMB1|",7)==0)ncfg->CheatPath[0]=0;\n'
        '\t\t\t\tint ret = SelectGame();\n'
        '\t\t\t\tif (ret & 2) SaveSettings = true;\n'
        '\t\t\t\tif (ret & 1) break;\n'
        '\t\t\t}\n'
        '\t\t\tredraw = true;\n'
        '\t\t}\n'
        '\t\telse if (FPAD_Start(0))\n'
        '\t\t{\n'
        '\t\t\tShowMessageScreenAndExit("Returning to loader...", 0);\n'
        '\t\t}\n'
        '\t\telse if (FPAD_Down(0))\n'
        '\t\t{\n'
        '\t\t\tif(maizo_selection<2)maizo_selection++;\n'
        '\t\t\tif(maizo_selection==1)ncfg->Config |= NIN_CFG_USB;\n'
        '\t\t\tredraw = true;\n'
        '\t\t}\n'
        '\t\telse if (FPAD_Up(0))\n'
        '\t\t{\n'
        '\t\t\tif(maizo_selection>0)maizo_selection--;\n'
        '\t\t\tif(maizo_selection==0)ncfg->Config &= ~NIN_CFG_USB;\n'
        '\t\t\tredraw = true;\n'
        '\t\t}')
    print("MaizoNintendont: independent SMB browser/config editor installed")

    print("Nintendont: experimental SMB1 guest ISO backend patched into ISO.c")

def main() -> int:
    if len(sys.argv)!=2:
        raise SystemExit("usage: patch_nintendont_maizonet.py UPSTREAM_NINTENDONT")
    root=Path(sys.argv[1]).resolve()
    # GCC14 refuses the old void (*)() signature with -Werror. This merely
    # specifies the exact function pointer types used by the apploader ABI.
    replace_one(
        root/"multidol"/"apploader.c",
        "typedef void  (*app_entry)(void (**init)(int (*report)(const char *fmt, ...)), int (**main)(), void *(**final)());",
        "typedef void  (*app_entry)(app_init *init, app_main *main, app_final *final);"
    )
    print("Nintendont pinned source: corrected strict apploader ABI prototypes")
    patch_nin_kernel(root)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
