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
    for name in ("maizonet_smb1.h","maizonet_smb1.c",
                 "maizonet_nintendont_net.h","maizonet_nintendont_net.c",
                 "maizonet_net_ipv4.h"):
        shutil.copyfile(src / name,kernel / name)

    replace_one(
        kernel / "Makefile",
        "SDI.o usb.o usbstorage.o wdvd.o sock.o",
        "SDI.o usb.o usbstorage.o wdvd.o sock.o maizonet_smb1.o maizonet_nintendont_net.o"
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
    replace_one(loader,
        '\t// Get multi-game and region code information.',
        '\tif(strncmp(ncfg->CheatPath,"MZSMB1|",7)==0)\n'
        '\t{\n'
        '\t\t/* libogc net_init: NCD link, NWC24 startup, SO, DHCP. */\n'
        '\t\tint net_status = net_init();\n'
        '\t\tgprintf("MZNET: PPC network preflight=%d\\r\\n",net_status);\n'
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
        '\t\t\t\t\t\t\tmaizonet_nin_fatal("ISO_OPEN",0,0);\n'
        '\t\t\t\t\t\t_sprintf( GamePath, "%s", ConfigGetGamePath() );')

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
