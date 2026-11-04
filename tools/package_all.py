#!/usr/bin/env python3
"""Reproducible-ish Wii media bundle: official upstream releases pinned by tag.
The bundles are UNMODIFIED upstream binaries. HID support is module-dependent.
Fails closed if a release is missing/ambiguous; never ships a fake boot.dol.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import time
import urllib.request
import zipfile

MAX_DL = 45 * 1024 * 1024
RELEASES = {
    "usbloader_gx": ("wiidev/usbloadergx", "v4.0-r1283", "usbloadergx_r1283.zip"),
    "WiiSXRX": ("niuus/WiiSXRX", "v3.3", "WiiSXRX-3.3-FS.zip"),
    "snes9xgx": ("dborth/snes9xgx", "5.0.5", "Snes9xGX-5.0.5-Wii.zip"),
    "fceugx": ("dborth/fceugx", "4.0.4", "FCEUltraGX-4.0.4-Wii.zip"),
    "vbagx": ("dborth/vbagx", "3.0.5", "VisualBoyAdvanceGX-3.0.5-Wii.zip"),
    "WiiMC": ("SuperrSonic/WiiMC-SSLC", "v3.0.0-2024", "WiiMC-SS_3.0.0_2024_v1.7z"),
}
NINTENDONT_REF = "c683db301c941a64e6fe5cef6ac079bd1a726074"
NINTENDONT_BASE = "https://raw.githubusercontent.com/FIX94/Nintendont/" + NINTENDONT_REF + "/"
NINTENDONT_PROFILE = "controllerconfigs/controller_Genreic_USB_Gamepad_PID=0079_VID=0006.ini"

def download(url: str, path: Path):
    last = None
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "MAIZO-Wii-Bundler/1.0"})
            with urllib.request.urlopen(req, timeout=90) as response:
                length = response.headers.get("Content-Length")
                if length and int(length) > MAX_DL:
                    raise ValueError("oversized download")
                with path.open("wb") as out:
                    total = 0
                    while True:
                        chunk = response.read(1024 * 1024)
                        if not chunk:
                            break
                        total += len(chunk)
                        if total > MAX_DL:
                            raise ValueError("download exceeds limit")
                        out.write(chunk)
            if path.stat().st_size == 0:
                raise ValueError("empty download")
            return {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        except (OSError, ValueError) as exc:
            last = exc
            path.unlink(missing_ok=True)
            if attempt < 2:
                time.sleep(3 + attempt * 4)
    raise RuntimeError("Unable to download " + url + ": " + str(last))

def safe_extract(source: Path, destination: Path):
    """Extract release without path traversal; avoid symlinks and giant payloads."""
    if source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            members = archive.infolist()
            total = 0
            seen = {}
            # Validate every member before writing anything. ZIP's extractall
            # normalizes names differently on different host platforms.
            for member in members:
                p = PurePosixPath(member.filename.replace("\\", "/"))
                raw_parts = member.filename.replace("\\", "/").rstrip("/").split("/")
                # PurePosixPath silently drops dot/empty components. Reject
                # ambiguous names before normalization or collision checks.
                if any(part in ("", ".", "..") for part in raw_parts):
                    raise ValueError("ambiguous ZIP member path")
                parts = p.parts
                if (not parts or p.is_absolute() or ".." in parts
                        or chr(0) in member.filename
                        or (len(parts[0]) >= 2 and parts[0][0].isalpha()
                            and parts[0][1] == ":")):
                    raise ValueError("unsafe ZIP member")
                # FAT32 cannot safely represent trailing dots/spaces, DOS device
                # names or characters forbidden by Windows-compatible tools.
                # Reject them before writing any archive member.
                for component in parts:
                    base = component.split(".")[0].upper()
                    if (component in (".", "") or component.rstrip(" .") != component
                            or any(ch in component for ch in '<>:"|?*')
                            or any(ord(ch) < 32 for ch in component)
                            or base in {"CON", "PRN", "AUX", "NUL"}
                            or (len(base) == 4 and base[:3] in ("COM", "LPT")
                                and base[3] in "123456789")):
                        raise ValueError("ZIP name incompatible with FAT32")
                key = "/".join(parts).casefold()
                # A file cannot also be a parent directory. Detect conflicts
                # before extraction so order cannot affect the result.
                kind = "dir" if member.is_dir() else "file"
                if key in seen:
                    raise ValueError("duplicate ZIP member")
                for index in range(1, len(parts)):
                    parent = "/".join(parts[:index]).casefold()
                    if seen.get(parent) == "file":
                        raise ValueError("ZIP parent is a file")
                if kind == "file" and any(existing.startswith(key + "/") for existing in seen):
                    raise ValueError("ZIP file shadows directory")
                seen[key] = kind
                mode = (member.external_attr >> 16) & 0o170000
                if member.create_system == 3 and mode not in (0, 0o100000, 0o040000):
                    raise ValueError("unsupported ZIP file type")
                if member.file_size > 0 and member.compress_size == 0:
                    raise ValueError("invalid ZIP compression size")
                total += member.file_size
                if total > 160 * 1024 * 1024:
                    raise ValueError("ZIP content exceeds limit")
            destination.mkdir(parents=True, exist_ok=True)
            for member in members:
                p = PurePosixPath(member.filename.replace("\\", "/"))
                target = destination.joinpath(*p.parts)
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(member) as src, target.open("xb") as dst:
                        shutil.copyfileobj(src, dst)
    elif source.suffix.lower() == ".7z":
        subprocess.run(["7z", "x", "-y", str(source), "-o" + str(destination)], check=True,
                       stdout=subprocess.DEVNULL, timeout=120)
        for p in destination.rglob("*"):
            if p.is_symlink() or not p.resolve().is_relative_to(destination.resolve()):
                raise ValueError("unsafe 7z member")
    else:
        raise ValueError("unknown release archive")

def valid_dol(data: bytes):
    """DOL has no magic; validate nonempty sections and entrypoint sanity."""
    if len(data) < 256:
        return False
    import struct
    words = struct.unpack_from(">57I", data, 0)
    entry = words[56]
    # 7 text + 11 data section offsets, addresses, lengths
    offsets = words[:18]
    addresses = words[18:36]
    sizes = words[36:54]
    sections = 0
    if not (0x80000000 <= entry < 0x94000000):
        return False
    for offset, addr, size in zip(offsets, addresses, sizes):
        if not size:
            continue
        sections += 1
        if not (offset >= 0x100 and offset + size <= len(data)
                and 0x80000000 <= addr < 0x94000000
                and addr + size <= 0x94000000):
            return False
    return sections > 0

def choose_dol(src: Path, name: str):
    candidates = [p for p in src.rglob("*") if p.is_file() and p.suffix.lower() == ".dol"]
    # Prefer the Wii Homebrew Channel boot.dol, not a channel forwarder.
    def score(p: Path):
        parts = [x.lower() for x in p.relative_to(src).parts]
        joined = "/".join(parts)
        expected = name.lower()
        banned = any(x in joined for x in ("gamecube", "wiiu", "channel-installer", "forwarder", "fwd"))
        return (-int(banned), int("apps" in parts), int(expected in joined),
                int(p.name.lower() == "boot.dol"), -len(parts))
    candidates.sort(key=score, reverse=True)
    if not candidates:
        raise ValueError(name + ": archive has no DOL")
    safe_candidates = [p for p in candidates if score(p)[0] == 0]
    if not safe_candidates:
        raise ValueError(name + ": only forwarder/GameCube/WiiU DOL found")
    # A release may contain an invalid preview/placeholder alongside a valid
    # Wii executable. Validate all eligible candidates before failing.
    for chosen in safe_candidates:
        if valid_dol(chosen.read_bytes()):
            return chosen
    raise ValueError(name + ": no valid Wii DOL among " +
                     str(len(safe_candidates)) + " candidates")

def copy_module(src: Path, dst: Path, name: str):
    chosen = choose_dol(src, name)
    dst.mkdir(parents=True, exist_ok=True)
    # Keep icon/meta and adjacent runtime files if present.
    if chosen.name.lower() == "boot.dol":
        for entry in chosen.parent.iterdir():
            if entry.is_file() and entry.suffix.lower() in (".dol", ".xml", ".png", ".cfg", ".txt"):
                shutil.copy2(entry, dst / entry.name)
    shutil.copy2(chosen, dst / "boot.dol")
    return {"from": str(chosen.relative_to(src)), "bytes": (dst / "boot.dol").stat().st_size}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frontend", type=Path, default=Path("boot.dol"))
    parser.add_argument("--out", type=Path, default=Path("dist"))
    args = parser.parse_args()
    assert valid_dol(args.frontend.read_bytes()), "Frontend DOL is invalid"
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    (out / "apps/maizo").mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.frontend, out / "apps/maizo/boot.dol")
    shutil.copy2("apps/maizo/meta.xml", out / "apps/maizo/meta.xml")
    (out / "maizo/controllers").mkdir(parents=True, exist_ok=True)
    shutil.copy2("profiles/0079_0006.ini", out / "maizo/controllers/0079_0006.ini")
    (out / "maizo/tools").mkdir(parents=True, exist_ok=True)
    for helper in ("cover_sync.py", "build_cover_cache.py"):
        shutil.copy2(Path("tools") / helper, out / "maizo/tools" / helper)

    manifest = {"frontend": "MAIZO-WII", "upstream": {}, "hid": {
        "maizo": "Microntek 0079:0006 + configurable HID profiles",
        "other_modules": "Official unmodified binary; support varies per upstream",
    }}
    with tempfile.TemporaryDirectory() as temp:
        tmp = Path(temp)
        for name, (owner, tag, asset) in RELEASES.items():
            url = f"https://github.com/{owner}/releases/download/{tag}/{asset}"
            downloaded = tmp / asset
            checksum = download(url, downloaded)
            unpacked = tmp / ("unpacked-" + name)
            unpacked.mkdir()
            safe_extract(downloaded, unpacked)
            details = copy_module(unpacked, out / "apps" / name, name)
            manifest["upstream"][name] = {"source": owner, "tag": tag,
                                           "archive": checksum, "binary": details}
            print(name, details, flush=True)
            # Preserve accessory files where the archive has conventional root folders.
            for d in unpacked.rglob("*"):
                if d.is_dir() and d.name.lower() == name.lower() and d.parent.name.lower() != "apps":
                    # Do not overwrite an HBC app or copy ROMs.
                    if len(d.relative_to(unpacked).parts) <= 2:
                        for f in d.rglob("*"):
                            if not f.is_file() or f.suffix.lower() not in (".cfg", ".xml", ".txt", ".png", ".ini"):
                                continue
                            rel = f.relative_to(d)
                            target = out / name / rel
                            target.parent.mkdir(parents=True, exist_ok=True)
                            shutil.copy2(f, target)
        # Bundle the Nintendont community controller database used by its own HID driver.
        controller_zip = tmp / "nin-controllers.zip"
        manifest["upstream"]["nintendont:controllers.zip"] = download(
            NINTENDONT_BASE + "controllerconfigs/controllers.zip", controller_zip)
        controller_src = tmp / "nintendont-profiles"
        controller_src.mkdir()
        safe_extract(controller_zip, controller_src)
        import re
        copied = 0
        for ini in controller_src.rglob("*.ini"):
            if not re.fullmatch(r"[A-Fa-f0-9]{4}_[A-Fa-f0-9]{4}\.ini", ini.name):
                continue
            dest = out / "controllers" / ini.name.upper().replace(".INI", ".ini")
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists():
                shutil.copy2(ini, dest)
                copied += 1
        manifest["hid"]["nintendont_database_profiles"] = copied
        for path, rel in (
            ("loader/loader.dol", "apps/Nintendont/boot.dol"),
            ("nintendont/meta.xml", "apps/Nintendont/meta.xml"),
            (NINTENDONT_PROFILE, "controllers/0079_0006.ini"),
        ):
            dest = out / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            data = download(NINTENDONT_BASE + path, dest)
            manifest["upstream"]["nintendont:" + path] = data
        if not valid_dol((out / "apps/Nintendont/boot.dol").read_bytes()):
            raise ValueError("Nintendont DOL failed validation")
        # Make launcher module paths an explicit invariant.
        expected = {"usbloader_gx", "Nintendont", "WiiSXRX", "snes9xgx", "fceugx", "vbagx", "WiiMC"}
        for name in expected:
            assert valid_dol((out / "apps" / name / "boot.dol").read_bytes()), name
        (out / "maizo").mkdir(exist_ok=True)
        (out / "maizo/MODULE_MANIFEST.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        (out / "README-INSTALAR.txt").write_text(
            "MAIZO Wii ALL-IN-ONE - extrair na RAIZ do SD ou USB FAT32.\n"
            "Inclui DOLs oficiais sem patches HID entre modulos.\n"
            "Os jogos, BIOS e chaves Wii/cIOS NAO estao incluidos.\n"
            "Nem todos os gamepads compativeis com MAIZO funcionam dentro dos emuladores.\n"
            "Consulte docs do repositorio para limitacoes e testes.\n",
            encoding="utf-8")
        print("PASS bundle: all seven external module executables validated")

if __name__ == "__main__":
    main()
