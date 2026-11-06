#!/usr/bin/env python3
"""Prepare the MaizoWii platform carousel from KyleBing's CC BY 4.0 icons.

Downloads happen on CI or a PC, NEVER on the Wii rendering thread.
Output is native MZG1/YUYV-compatible cache (128x176, GX tiled RGBA8).
All supplied icons retain their original proportions.
"""
import argparse
import io
import struct
from pathlib import Path
import urllib.request

from PIL import Image, ImageDraw, ImageFont, ImageOps
from build_cover_cache import pack_rgba, W, H

REV = "b4facf0720a0bfaa8befc8234d714b29141f3b99"
BASE = ("https://raw.githubusercontent.com/KyleBing/retro-game-console-icons/"
        + REV + "/series_trimui/300w@1x/")
# Current KyleBing collection has no Nintendo Wii portrait.
ICONS = {
    "IGC": "NGC", "IPS1": "PS", "ISNES": "SFC",
    "INES": "FC", "IGBA": "GBA", "IGB": "GB", "IMEDIA": "VIDEOS",
}

def background():
    canvas = Image.new("RGBA", (W, H))
    p = canvas.load()
    for y in range(H):
        for x in range(W):
            tone = int(23 + 11 * y / H + (W // 2 - abs(x - W // 2)) / 35)
            p[x, y] = (tone, tone + 5, tone + 12, 255)
    return canvas

def portrait(name):
    if name in ("IWII", "IHB"):
        im = Image.new("RGBA", (200, 200), (0, 0, 0, 0))
        draw = ImageDraw.Draw(im)
        try:
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 68)
        except OSError:
            font = ImageFont.load_default()
        draw.rounded_rectangle((16, 17, 184, 183), radius=25, fill=(226, 233, 238, 255))
        box = draw.textbbox((0, 0), "Wii" if name == "IWII" else "HB", font=font)
        tw, th = box[2] - box[0], box[3] - box[1]
        draw.text(((200 - tw)//2 - box[0], (200 - th)//2 - box[1]),
                  "Wii" if name == "IWII" else "HB",
                  fill=(44, 64, 88, 255), font=font)
        return im
    name = ICONS[name]
    url = BASE + name + ".png"
    request = urllib.request.Request(url, headers={"User-Agent": "MaizoWii/1.0 (+https://github.com/maizon/MaizoWii)"})
    with urllib.request.urlopen(request, timeout=20) as response:
        raw = response.read(2_000_001)
        if len(raw) > 2_000_000:
            raise ValueError("Icon exceeds size limit: " + name)
    with Image.open(io.BytesIO(raw)) as source:
        source.load()
        return source.convert("RGBA")

def transparent_pack(im):
    """GX RGBA8 tiles preserving alpha for true masked topbar icons."""
    px=im.load()
    output=bytearray(b"MZG1"+struct.pack(">HH",W,H))
    for ty in range(0,H,4):
        for tx in range(0,W,4):
            data=[px[tx+x,ty+y] for y in range(4) for x in range(4)]
            for r,g,b,a in data: output.extend((a,r))
            for r,g,b,a in data: output.extend((g,b))
    return bytes(output)

def render_icon(code, transparent=False):
    canvas = Image.new("RGBA",(W,H),(0,0,0,0)) if transparent else background()
    if transparent and code in ("IWII","IHB"):
        # The home portraits keep their rounded cards, but the tiny topbar
        # sprites are just the glyph silhouette: NO box baked into alpha.
        original = Image.new("RGBA", (200,200), (0,0,0,0))
        draw = ImageDraw.Draw(original)
        word="Wii" if code=="IWII" else "HB"
        try:
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 72)
        except OSError:
            font = ImageFont.load_default()
        box=draw.textbbox((0,0),word,font=font)
        draw.text(((200-(box[2]-box[0]))//2-box[0],
                   (200-(box[3]-box[1]))//2-box[1]),
                  word,font=font,fill=(239,245,249,255))
    else:
        original = portrait(code)
    fitted = ImageOps.contain(original, (112, 148), Image.Resampling.LANCZOS)
    canvas.alpha_composite(fitted, ((W - fitted.width)//2, (H - fitted.height)//2))
    return transparent_pack(canvas) if transparent else pack_rgba(canvas)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", type=Path, required=True, help="Output maizo/covers folder")
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for code in ("IWII", "IHB", *ICONS):
        for transparent in (False,True):
            asset=("T" if transparent else "I")+code[1:]
            dest = args.out / (asset + ".gxt")
            data = render_icon(code,transparent)
            tmp = dest.with_suffix(".part")
            tmp.write_bytes(data)
            tmp.replace(dest)
            print("ICON", asset, len(data))
    license_file = args.out.parent / "SYSTEM_ICONS_CREDITS.txt"
    license_file.write_text(
        "MAIZO original UI adaptation of retro-game-console-icons by KyleBing.\n"
        "https://github.com/KyleBing/retro-game-console-icons\n"
        "Source revision: " + REV + "\n"
        "Source images (all except Wii): CC BY 4.0; attribution required.\n"
        "https://creativecommons.org/licenses/by/4.0/\n"
        "Modified by Maizo: resized, padded, composited and packed to native MZG1.\n"
        "Wii and HB images: locally drawn fallback icons (not in the source icon set).\n",
        encoding="utf-8",
    )

if __name__ == "__main__":
    main()
