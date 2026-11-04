#!/usr/bin/env python3
"""Download legally available Wii/GameCube box art using GameTDB disc IDs.

Run on a PC or rooted Android with an attached FAT32 storage volume:
python3 tools/cover_sync.py /media/WII --region US
python3 tools/build_cover_cache.py /media/WII/maizo/covers
"""
import argparse
import json
import pathlib
import re
import time
import urllib.request

PNG = b"\x89PNG\r\n\x1a\n"
ID = re.compile(rb"[A-Za-z0-9]{6}")
FOLDER_ID = re.compile(r"\[([A-Za-z0-9]{6})\]")

def images(root, kind):
    """Return ID6 keys without downloading or processing game data."""
    seen = set()
    if kind == "gc":
        candidates = root.rglob("*")
        formats = {".iso", ".gcm", ".ciso"}
    else:
        candidates = root.rglob("*")
        formats = {".iso", ".wbfs"}
    for p in candidates:
        if not p.is_file() or p.suffix.lower() not in formats:
            continue
        code = None
        if kind == "wii":
            match = FOLDER_ID.search(p.parent.name)
            if match:
                code = match.group(1).encode()
            elif ID.fullmatch(p.stem.encode("ascii", "ignore")):
                code = p.stem.encode("ascii")
        if code is None and (p.suffix.lower() == ".iso" or kind == "gc"):
            try:
                with p.open("rb") as f:
                    code = f.read(6)
            except OSError:
                continue
        if code and ID.fullmatch(code):
            key = code.decode("ascii").upper()
            if key not in seen:
                seen.add(key)
                yield key

def download(url, dest):
    request = urllib.request.Request(url, headers={
        "User-Agent": "MAIZO-Wii-CoverSync/0.2 (https://github.com/maizon/MaizoWii)"
    })
    with urllib.request.urlopen(request, timeout=15) as response:
        if response.status != 200:
            return False
        data = response.read(2_000_001)
    if len(data) > 2_000_000 or not data.startswith(PNG):
        return False
    tmp = dest.with_suffix(".part")
    tmp.write_bytes(data)
    tmp.replace(dest)
    return True

def main():
    p = argparse.ArgumentParser()
    p.add_argument("device", type=pathlib.Path, help="USB or SD root")
    p.add_argument("--region", default="US", choices=["US","EN","FR","DE","ES","IT","JP","BR"])
    p.add_argument("--limit", type=int, default=150)
    p.add_argument("--systems", choices=["all", "gc", "wii"], default="all")
    a = p.parse_args()
    out = a.device / "maizo" / "covers"
    out.mkdir(parents=True, exist_ok=True)
    stats = {"downloaded": 0, "existing": 0, "missing": 0}
    pairs = [("gc",a.device/"games"), ("wii",a.device/"wbfs")]
    for kind, game_root in pairs:
        if a.systems not in ("all", kind) or not game_root.is_dir():
            continue
        for key in images(game_root, kind):
            if sum(stats.values()) >= a.limit:
                break
            target = out / (key + ".png")
            if target.exists():
                stats["existing"] += 1
                continue
            success = False
            for region in dict.fromkeys((a.region, "US")):
                url = f"https://art.gametdb.com/{kind}/cover/{region}/{key}.png"
                try:
                    if download(url, target):
                        success = True
                        break
                except (OSError, ValueError) as exc:
                    print(f"WARN {kind}/{key}/{region}: {exc}")
            stats["downloaded" if success else "missing"] += 1
            print(f"{kind.upper()} {key}: {'OK' if success else 'not found'}")
            time.sleep(0.15)
    print(json.dumps(stats, indent=2))

if __name__ == "__main__":
    main()
