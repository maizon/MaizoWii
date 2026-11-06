#!/usr/bin/env python3
"""Import official DAT metadata into Maizo's offline per-system title index.

Only text names are processed: game ROM content is never downloaded or read.
Usage: python3 tools/import_nointro_dat.py SNES your-dat.xml --out titles.tsv
"""
import argparse
import re
from pathlib import Path
import xml.etree.ElementTree as ET

SYSTEMS = ("WII", "GC", "PS1", "SNES", "NES", "GBA", "GB")
REGIONS = (("USA","USA"),("Europe","EUROPA"),("Japan","JAPAO"),
           ("Brazil","BRASIL"),("World","MUNDO"))
def clean(value):
    return value.replace("\t"," ").replace("\r"," ").replace("\n"," ").strip()

def import_dat(system, source):
    result = {}
    for event, elem in ET.iterparse(source,events=("end",)):
        if elem.tag not in ("game","machine"):
            continue
        title=clean(elem.attrib.get("name",""))
        if not title:
            elem.clear()
            continue
        area=next((label for key,label in REGIONS if f"({key})" in title),"")
        year=elem.attrib.get("year","")
        year=year if re.fullmatch(r"19\d\d|20\d\d",year) else ""
        for rom in elem.findall("rom"):
            filename=clean(rom.attrib.get("name",""))
            if not filename:
                continue
            key=Path(filename).stem
            if key.casefold()==title.casefold():
                continue # already represented by filename fallback
            if key:
                result[(system,key)] = (title,year,area)
        elem.clear()
    return result

def main():
    p=argparse.ArgumentParser()
    p.add_argument("system",choices=SYSTEMS)
    p.add_argument("dat",type=Path)
    p.add_argument("--out",type=Path,default=Path("titles.tsv"))
    args=p.parse_args()
    original={}
    if args.out.exists():
        for line in args.out.read_text(encoding="utf-8").splitlines():
            bits=line.split("\t")
            if len(bits)>=3:
                original[(bits[0],bits[1])]=(bits[2],bits[3] if len(bits)>3 else "",bits[4] if len(bits)>4 else "")
    original.update(import_dat(args.system,args.dat))
    tmp=args.out.with_suffix(args.out.suffix+".part")
    with tmp.open("w",encoding="utf-8",newline="\n") as out:
        out.write("# SYSTEM<TAB>ROM_BASENAME_OR_ID<TAB>OFFICIAL_TITLE<TAB>YEAR<TAB>REGION\n")
        for (system,key),(title,year,area) in sorted(original.items()):
            out.write("\t".join(map(clean,(system,key,title,year,area)))+"\n")
    tmp.replace(args.out)
    print(f"Maizo offline title records: {len(original)} -> {args.out}")

if __name__=="__main__": main()
