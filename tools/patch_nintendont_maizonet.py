#!/usr/bin/env python3
"""Patch pinned FIX94/Nintendont for MaizoNet experiments.

Keep Nintendont kernel/build isolated from the MaizoWii frontend.
Important: this does NOT currently enable network GameCube play.
Do not package as production unless the kernel stream backend has real tests.
"""
from pathlib import Path
import sys

def replace_one(path: Path, before: str, after: str) -> None:
    s=path.read_text(encoding="utf-8")
    if s.count(before)!=1:
        raise RuntimeError(f"{path}: expected exactly one matching anchor, got {s.count(before)}")
    path.write_text(s.replace(before,after),encoding="utf-8")

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
    return 0

if __name__=="__main__":
    raise SystemExit(main())
