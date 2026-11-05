#!/usr/bin/env python3
"""Fail WiiMC builds with newlib's weak, stack-limited _sbrk_r.

The Dolphin 2026-11-05 trace resolves PC 0x80075360 to
__VIClearFramebuffer.  Its target 0x40000000 is MEM_K0_TO_K1(NULL),
i.e. the framebuffer allocator returned NULL.  The matching debug ELF
previously exposed "W _sbrk_r" plus an unreferenced __syscall_sbrk_r,
which meant the historical libogc allocator was bypassed.
"""
import os
import pathlib
import subprocess
import sys


def require_strong_libogc_sbrk(elf: pathlib.Path, nm: str) -> None:
    result = subprocess.run(
        [nm, "-n", str(elf)], text=True, check=True,
        capture_output=True
    )
    symbols = {}
    for line in result.stdout.splitlines():
        words = line.split()
        if len(words) == 3 and len(words[1]) == 1:
            symbols[words[2]] = words[1]
    needed = {
        "_sbrk_r": "T",
        "__syscall_sbrk_r": "T",
        "__libogc_sbrk_r": "T",
        "SYS_AllocateFramebuffer": "T",
        "__syscalls": "B",
    }
    mismatches = {name: (symbol_type, symbols.get(name))
                  for name, symbol_type in needed.items()
                  if symbols.get(name) != symbol_type}
    if mismatches:
        raise RuntimeError(
            "FAIL WiiMC heap ABI: " + repr(mismatches) +
            "; a weak _sbrk_r can exhaust the short newlib stack heap "
            "and pass NULL to VIDEO_ClearFrameBuffer"
        )
    print("PASS WiiMC ABI: strong _sbrk_r routes via libogc SYS_Init",
          "handlers; original video/codec engine unchanged")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_wiimc_heap.py wiimc.elf")
    nm = os.environ.get(
        "PPC_NM", "/opt/devkitpro/devkitPPC/powerpc-eabi/bin/nm"
    )
    require_strong_libogc_sbrk(pathlib.Path(sys.argv[1]), nm)
