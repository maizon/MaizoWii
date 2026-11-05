#!/usr/bin/env python3
"""Structural DOL verifier for standalone WiiMC. No emulation claim."""
import argparse
import struct
from pathlib import Path

def inspect_dol(blob):
    if len(blob) < 0x100:
        raise ValueError("truncated DOL header")
    u=lambda off: struct.unpack_from(">I",blob,off)[0]
    segs=[]
    for typ,num,offs,addrs,sizes in (
        ("text",7,0x00,0x48,0x90),
        ("data",11,0x1c,0x64,0xac),
    ):
        for i in range(num):
            ofs,addr,size=(u(offs+i*4),u(addrs+i*4),u(sizes+i*4))
            if size==0:
                continue
            if ofs<0x100 or ofs>len(blob) or size>len(blob)-ofs:
                raise ValueError(f"{typ}{i} truncated in file")
            if addr<0x80003100 or addr>=0x94000000 or size>0x94000000-addr:
                raise ValueError(f"{typ}{i} invalid Wii RAM address")
            segs.append((typ,i,ofs,addr,size))
    if not segs:
        raise ValueError("no loaded DOL sections")
    for k,a in enumerate(segs):
        for b in segs[k+1:]:
            if max(a[3],b[3])<min(a[3]+a[4],b[3]+b[4]):
                raise ValueError(f"DOL sections overlap: {a[0]}{a[1]} and {b[0]}{b[1]}")
    entry=u(0xe0)
    if not any(t=="text" and addr<=entry<addr+size for t,_,_,addr,size in segs):
        raise ValueError("entrypoint outside executable text")
    bss_start,bss_size=u(0xd8),u(0xdc)
    if bss_size:
        if bss_start<0x80003100 or bss_start>=0x94000000 or bss_size>0x94000000-bss_start:
            raise ValueError("BSS outside Wii RAM")
        for t,i,_,addr,size in segs:
            if max(addr,bss_start)<min(addr+size,bss_start+bss_size):
                raise ValueError(f"BSS overlaps {t}{i}")
    return segs,entry,bss_start,bss_size

def main():
    p=argparse.ArgumentParser()
    p.add_argument("dol",type=Path)
    a=p.parse_args()
    segments,entry,bss,bss_len=inspect_dol(a.dol.read_bytes())
    print(f"PASS WiiMC DOL loaded_sections={len(segments)} entry={entry:#x} bss={bss:#x}+{bss_len:#x}")
    for t,i,ofs,addr,size in segments:
        print(f"  {t}{i} file={ofs:#x} load={addr:#x} size={size:#x}")
    if bss_len==0:
        print("NOTE: header BSS=0; historical WiiMC CRT0 must explicitly clear its linker BSS; this is not runtime proof.")
if __name__=="__main__":
    main()
