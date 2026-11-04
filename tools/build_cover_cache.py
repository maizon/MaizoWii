#!/usr/bin/env python3
"""Convert downloaded PNG covers to native 128x160 tiled GX_RGBA8 for MAIZO Wii.
Requires: python -m pip install pillow. All work happens on a PC/Android.
"""
from pathlib import Path
import argparse,struct
try: from PIL import Image
except ImportError: raise SystemExit("Pillow required: python -m pip install pillow")
W,H=128,160
def pack_rgba(im):
    im=im.convert("RGBA").resize((W,H),Image.Resampling.LANCZOS)
    pix=im.load()
    out=bytearray(b"MZG1"+struct.pack(">HH",W,H))
    for ty in range(0,H,4):
        for tx in range(0,W,4):
            pixels=[pix[tx+x,ty+y] for y in range(4) for x in range(4)]
            for r,g,b,a in pixels:out.extend((a,r))
            for r,g,b,a in pixels:out.extend((g,b))
    assert len(out)==8+W*H*4
    return bytes(out)
def main():
    p=argparse.ArgumentParser()
    p.add_argument("covers",type=Path,help="maizo/covers directory")
    args=p.parse_args()
    count=0
    for src in args.covers.glob("*.png"):
        dest=src.with_suffix(".gxt")
        try:
            with Image.open(src) as im: data=pack_rgba(im)
            dest.write_bytes(data);print(f"OK {dest.name} ({len(data)} bytes)")
            count+=1
        except Exception as e:print(f"WARN {src}: {e}")
    print(f"Converted {count} covers")
if __name__=="__main__":main()
