#!/usr/bin/env python3
"""Replace official modules with successfully built HID-adapted modules.

Never copy a missing/corrupt build. A failed adapter automatically retains the
official upstream DOL and marks fallback in the install manifest.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
import sys
from package_all import valid_dol

MODULES=("usbloader_gx","WiiSXRX","WiiMC","snes9xgx","fceugx","vbagx")

def overlay(adapter_root: Path, dist: Path) -> dict[str,str]:
    manifest_path=dist/"maizo/MODULE_MANIFEST.json"
    manifest=json.loads(manifest_path.read_text(encoding="utf-8"))
    # Rebuild provenance from this invocation, never trust a prior manifest.
    # Otherwise a second packaging pass can claim an official fallback as HID.
    previous_adapted = manifest.get("adapted_sha256", {})
    # An in-place rerun may contain an adapted DOL from a previous pass.
    # If its source disappears, do not relabel that binary as official.
    # A clean package_all.py staging pass is required to restore fallback.
    for module in MODULES:
        previous_hash = previous_adapted.get(module)
        candidate = dist / "apps" / module / "boot.dol"
        source = adapter_root / "apps" / module / "boot.dol"
        if previous_hash and not source.is_file() and candidate.is_file():
            current_hash = hashlib.sha256(candidate.read_bytes()).hexdigest()
            if current_hash == previous_hash:
                raise ValueError("Cannot relabel adapted DOL as official fallback: " + module +
                                 "; rebuild clean dist before overlay")
    # Validate every input before touching dist: a corrupt later adapter must
    # never leave an earlier module replaced with a stale manifest.
    prepared = {}
    for module in MODULES:
        source = adapter_root / "apps" / module / "boot.dol"
        destination = dist / "apps" / module / "boot.dol"
        if destination.is_symlink() or not destination.is_file():
            raise ValueError("Missing or unsafe staged module: " + module)
        if not source.exists() and not source.is_symlink():
            continue
        if source.is_symlink() or not source.is_file():
            raise ValueError("Unsafe adapted binary: " + str(source))
        contents = source.read_bytes()
        if len(contents) > 12 * 1024 * 1024 or not valid_dol(contents):
            raise ValueError("Invalid adapted module: " + module)
        prepared[module] = contents
    manifest["adapted_sha256"] = {}
    status = {}
    for module in MODULES:
        destination = dist / "apps" / module / "boot.dol"
        if module not in prepared:
            status[module] = "official (HID adapter binary unavailable)"
            continue
        contents = prepared[module]
        tmp = destination.with_suffix(".patched")
        tmp.write_bytes(contents)
        tmp.replace(destination)
        status[module] = "maizo HID source adapter (hardware tests still pending)"
        manifest["adapted_sha256"][module] = hashlib.sha256(contents).hexdigest()
    status["Nintendont"]="official own HID driver / controller.ini"
    # Record the final on-disk executable for every module, including fallbacks.
    # This is intentionally calculated AFTER overlays so the manifest cannot
    # describe a superseded upstream binary.
    all_modules=("maizo", "usbloader_gx", "Nintendont", "WiiSXRX",
                 "snes9xgx", "fceugx", "vbagx", "WiiMC")
    manifest["final_dol_sha256"]={name: hashlib.sha256(
        (dist/"apps"/name/"boot.dol").read_bytes()).hexdigest()
        for name in all_modules}
    manifest["module_input_provenance"]=status
    manifest["adapted_modules"]=sorted(name for name in MODULES if name in manifest.get("adapted_sha256", {}))
    manifest["fallback_modules"]=sorted(name for name in MODULES if name not in manifest.get("adapted_sha256", {}))
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    (dist / "HID-COMPATIBILITY.txt").write_text(
        "MAIZO: USB HID 0079:0006 and configurable profiles.\n" +
        "\n".join(f"{k}: {v}" for k, v in status.items()) +
        "\nHID support with unsupported USB protocols "
        "(0810:0001, XInput) is NOT guaranteed.\n",
        encoding="utf-8")
    return status

if __name__=="__main__":
    root=Path(sys.argv[1] if len(sys.argv)>1 else "patched")
    dist=Path(sys.argv[2] if len(sys.argv)>2 else "dist")
    for name,summary in overlay(root,dist).items():
        print(f"{name}: {summary}")
