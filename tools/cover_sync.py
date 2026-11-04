#!/usr/bin/env python3
"""Sync Wii/GameCube via GameTDB and retro-system cover art via Libretro.

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
import urllib.parse

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

# Public Libretro thumbnail library repo names (verified, master / Named_Boxarts).
LIBRETRO = {
    "ps1": ("Sony_-_PlayStation", ("ps1", "wiisxrx/isos", "roms/ps1"),
            (".cue", ".iso", ".img", ".bin", ".pbp")),
    "snes": ("Nintendo_-_Super_Nintendo_Entertainment_System",
             ("snes9xgx/roms", "roms/snes"), (".sfc", ".smc", ".fig", ".zip")),
    "nes": ("Nintendo_-_Nintendo_Entertainment_System",
            ("fceugx/roms", "roms/nes"), (".nes", ".fds", ".unf", ".zip")),
    "gba": ("Nintendo_-_Game_Boy_Advance",
            ("vbagx/roms", "roms/gba"), (".gba",)),
    "gb": ("Nintendo_-_Game_Boy",
           ("vbagx/roms", "roms/gb"), (".gb",)),
    "gbc": ("Nintendo_-_Game_Boy_Color",
            ("vbagx/roms", "roms/gb"), (".gbc",)),
}

def cover_key(name):
    """Match the firmware's cover key canonicalizer for games without disc IDs."""
    buf = []
    for ch in name.upper().replace("_", " "):
        if ("A" <= ch <= "Z") or ("0" <= ch <= "9"):
            buf.append(ch)
        elif ch == " " and buf and buf[-1] != "-":
            buf.append("-")
    return "".join(buf).rstrip("-")[:119]

def titles_for_lookup(name):
    """Try an exact Libretro title then normalized fallback variants."""
    name = name.replace("_", " ").strip()
    name = re.sub(r"\s+", " ", name)
    versions = [name]
    simplified = re.sub(r"\s*\[[^\]]*\]", "", name).strip()
    simplified = re.sub(r"\s*\((?:PT-?BR|BR|DUB|LEG|HACK|BETA)\)", "", simplified,
                        flags=re.I).strip()
    versions.append(simplified)
    plain = re.sub(r"\s*\((?:USA|U|EUR|EUROPE|E|JAPAN|J|WORLD)\)", "",
                   simplified, flags=re.I).strip()
    versions.extend([plain + " (USA)", plain + " (Europe)", plain])
    return list(dict.fromkeys(t for t in versions if t))[:5]

def named_roms(device, system):
    """Yield (cover_key, source title) for supported formats, unique by key."""
    _, paths, extensions = LIBRETRO[system]
    seen = set()
    for folder in paths:
        root = device / folder
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in extensions:
                continue
            if system == "ps1" and p.suffix.lower() == ".bin" and p.with_suffix(".cue").is_file():
                continue
            title = p.stem
            key = cover_key(title)
            if not key or key in seen:
                continue
            seen.add(key)
            yield key, title

def fetch_libretro(repo, title, dest):
    for candidate in titles_for_lookup(title):
        encoded = urllib.parse.quote(candidate + ".png", safe="")
        url = f"https://raw.githubusercontent.com/libretro-thumbnails/{repo}/master/Named_Boxarts/{encoded}"
        try:
            if download(url, dest):
                return True
        except (OSError, ValueError):
            pass
    return False

def main():
    p = argparse.ArgumentParser()
    p.add_argument("device", type=pathlib.Path, help="USB or SD FAT32 root")
    p.add_argument("--region", default="US", choices=["US","EN","FR","DE","ES","IT","JP","BR"])
    p.add_argument("--limit", type=int, default=150)
    p.add_argument("--systems", choices=["all", "gc", "wii", *LIBRETRO], default="all")
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
    for system, (repo, _, _) in LIBRETRO.items():
        if a.systems not in ("all", system):
            continue
        for key, title in named_roms(a.device, system):
            if sum(stats.values()) >= a.limit:
                break
            target = out / (key + ".png")
            if target.exists():
                stats["existing"] += 1
                continue
            success = fetch_libretro(repo, title, target)
            stats["downloaded" if success else "missing"] += 1
            print(f"{system.upper()} {title}: {'OK' if success else 'not found'}")
            time.sleep(0.1)
    print(json.dumps(stats,indent=2))

if __name__ == "__main__":
    main()
