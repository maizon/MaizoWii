#!/usr/bin/env python3
"""Retitle proven HBC-Reborn banner layouts without changing BRLYT/BRLAN code.

Only the two title textures are edited. Dimensions and color modes remain
unchanged, so the established layout and original animation remain intact.
HBC-Reborn licenses its banner source; credit it in distribution notices.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

def render(path: Path):
    with Image.open(path) as source:
        original_mode=source.mode
        width,height=source.size
    if width<64 or height<24 or width>2048 or height>2048:
        raise ValueError(f"Suspicious texture dimensions {width}x{height}")
    image=Image.new("RGBA",(width,height),(0,0,0,0))
    draw=ImageDraw.Draw(image)
    fontfile="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
    # Text is scaled to the original BRLYT title pane. Preserve alpha.
    font=None
    for size in range(max(12,int(height*0.7)),10,-1):
        sample=ImageFont.truetype(fontfile,size)
        box=draw.textbbox((0,0),"MAIZO",font=sample,stroke_width=1)
        if box[2]-box[0]<=width*0.9 and box[3]-box[1]<=height*0.75:
            font=sample
            break
    if font is None:
        raise ValueError("Cannot fit MAIZO into original title texture")
    box=draw.textbbox((0,0),"MAIZO",font=font,stroke_width=1)
    x=(width-(box[2]-box[0]))//2-box[0]
    y=(height-(box[3]-box[1]))//2-box[1]
    draw.text((x+1,y+2),"MAIZO",font=font,fill=(3,12,22,175),stroke_width=1,
              stroke_fill=(3,12,22,160))
    draw.text((x,y),"MAIZO",font=font,fill=(246,251,255,255),
              stroke_width=1,stroke_fill=(89,140,245,255))
    if original_mode=="RGB":
        bg=Image.new("RGB",image.size,(18,26,46))
        bg.paste(image,mask=image.getchannel("A"))
        image=bg
    elif original_mode=="RGBA":
        pass
    else:
        raise ValueError(f"Unexpected title texture color mode {original_mode}")
    image.save(path,format="PNG")
    check=Image.open(path)
    assert check.size==(width,height) and check.mode==original_mode
    print(f"MAIZO title texture: {path} ({width}x{height}, {original_mode})")

if __name__=="__main__":
    import sys
    root=Path(sys.argv[1])
    render(root/"channel/banner/icon/icon_title.png")
    render(root/"channel/banner/banner/banner_title.png")
    names=root/"channel/banner/names.txt"
    names.write_text("\n".join([
        "jp=Maizo Wii",
        "en=Maizo Wii",
        "de=Maizo Wii",
        "fr=Maizo Wii",
        "sp=Maizo Wii",
        "it=Maizo Wii",
        "nl=Maizo Wii",
        "cn=Maizo Wii",
        "ko=Maizo Wii",
    ])+"\n",encoding="utf-8")
