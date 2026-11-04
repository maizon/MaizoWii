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
    status={}
    for module in MODULES:
        source=adapter_root/"apps"/module/"boot.dol"
        destination=dist/"apps"/module/"boot.dol"
        if not source.is_file():
            status[module]="official (HID adapter binary unavailable)"
            continue
        if source.is_symlink():
            raise ValueError("Symlink adapted binary: "+str(source))
        contents=source.read_bytes()
        if len(contents)>12*1024*1024 or not valid_dol(contents):
            raise ValueError("Invalid adapted module: "+module)
        tmp=destination.with_suffix(".patched")
        tmp.write_bytes(contents)
        tmp.replace(destination)
        status[module]="maizo HID source adapter (hardware tests still pending)"
        manifest.setdefault("adapted_sha256",{})[module]=hashlib.sha256(contents).hexdigest()
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
