#!/usr/bin/env python3
"""Fail-closed audit for the ALL-IN-ONE package."""
import json
import hashlib
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
hashes = manifest.get("final_dol_sha256")
assert isinstance(hashes, dict) and set(hashes) == set(EXPECTED), "missing/extra final DOL hashes"
for name in EXPECTED:
    actual = hashlib.sha256((root / "apps" / name / "boot.dol").read_bytes()).hexdigest()
    assert hashes[name] == actual, "DOL SHA-256 mismatch: " + name
# Additional HBC DOL and GPL-licensed manager are independent of the
# eight core modules. Verify their provenance whenever the final manifest
# declares them; do not treat a naked DOL as an installable channel WAD.
fw = manifest.get("forwarder")
if fw:
    assert fw["path"] == "apps/maizo-forwarder/boot.dol", "forwarder path mismatch"
    fwd = root / fw["path"]
    assert valid_dol(fwd.read_bytes()), "INVALID forwarder DOL"
    assert hashlib.sha256(fwd.read_bytes()).hexdigest() == fw["sha256"], "forwarder hash mismatch"
    assert (root / "apps/maizo-forwarder/meta.xml").is_file(), "forwarder meta missing"
manager = manifest.get("installer")
if manager:
    assert manager["source"] == "modmii/YAWM-ModMii-Edition", "unapproved installer"
    assert manager["auto_installs"] is False, "unsafe automatic NAND install"
    assert manager["channel_wad_included"] is False, "unexpected WAD claim"
    exe = root / "apps/maizo-installer/boot.dol"
    assert valid_dol(exe.read_bytes()), "INVALID installer manager DOL"
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == manager["dol_sha256"], "installer hash mismatch"
    assert (root / "maizo/YAWM-LICENSE").is_file(), "installer GPL license missing"
    assert (root / "wad/LEIA-ANTES-DE-INSTALAR.txt").is_file(), "installer warning missing"
    assert (root / "wad/wm_config.txt").is_file(), "manager configuration missing"
    if "hid_adapted" in manager:
        assert isinstance(manager["hid_adapted"], bool), "installer HID status must be boolean"
        status = manifest.get("module_input_provenance", {}).get("maizo-installer", "")
        assert status, "installer HID status missing"
        if manager["hid_adapted"]:
            assert "HID source adapted" in status, "installer provenance mismatch"
        else:
            assert "official YAWM" in status, "installer fallback provenance mismatch"

files = list(root.rglob("*"))
assert not any(f.is_symlink() for f in files)
print("PASS: all eight application executables and controller profiles present")
