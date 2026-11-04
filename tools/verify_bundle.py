#!/usr/bin/env python3
"""Fail-closed audit for the ALL-IN-ONE package."""
import json
from pathlib import Path
import sys

from package_all import valid_dol

EXPECTED = ("maizo", "usbloader_gx", "Nintendont", "WiiSXRX",
            "snes9xgx", "fceugx", "vbagx", "WiiMC")
root = Path(sys.argv[1] if len(sys.argv) > 1 else "dist")
assert root.is_dir(), root
for name in EXPECTED:
    dol = root / "apps" / name / "boot.dol"
    assert dol.is_file(), "MISSING " + str(dol)
    assert dol.stat().st_size >= 256, "EMPTY " + str(dol)
    assert valid_dol(dol.read_bytes()), "INVALID " + str(dol)
for mandatory in ("apps/maizo/meta.xml", "controllers/0079_0006.ini",
                  "maizo/controllers/0079_0006.ini", "maizo/MODULE_MANIFEST.json"):
    assert (root / mandatory).is_file(), mandatory
manifest = json.loads((root / "maizo/MODULE_MANIFEST.json").read_text())
assert all(manifest["upstream"].get(name) for name in EXPECTED if name != "maizo" and name != "Nintendont")
files = list(root.rglob("*"))
assert not any(f.is_symlink() for f in files)
print("PASS: all eight application executables and controller profiles present")
