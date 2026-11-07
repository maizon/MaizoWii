#!/usr/bin/env python3
"""Fail-closed audit for the ALL-IN-ONE package."""
import json
import hashlib
from pathlib import Path
import sys

from package_all import valid_dol

EXPECTED = ("maizo", "usbloader_gx", "Nintendont", "WiiSXRX", "WiiSX",
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
# Prevent a successful bundle build from silently claiming HID coverage.
ADAPTABLE = {"usbloader_gx", "WiiSXRX", "WiiSX", "WiiMC", "snes9xgx", "fceugx", "vbagx"}
adapted = manifest.get("adapted_modules")
fallback = manifest.get("fallback_modules")
assert isinstance(adapted, list) and isinstance(fallback, list), "adapter inventory missing"
assert len(adapted) == len(set(adapted)) and len(fallback) == len(set(fallback)), "duplicate adapter inventory"
assert set(adapted).isdisjoint(fallback) and set(adapted) | set(fallback) == ADAPTABLE, "adapter inventory mismatch"
assert set(manifest.get("adapted_sha256", {})) == set(adapted), "adapted hash inventory mismatch"
for name in adapted:
    assert manifest["adapted_sha256"][name] == hashes[name], "adapted binary hash mismatch: " + name
for name in ADAPTABLE:
    provenance = manifest.get("module_input_provenance", {}).get(name, "")
    assert ("HID source adapter" in provenance) == (name in adapted), "adapter provenance mismatch: " + name
# Never distribute an unverified adapted WiiMC without the original
# working player as an integrity-protected recovery copy.
backup = manifest.get("wiimc_official_sha256")
if backup is not None:
    assert "WiiMC" in adapted, "WiiMC recovery copy without adapted binary"
    original = root / "apps/WiiMC-official/boot.dol"
    assert original.is_file() and valid_dol(original.read_bytes()), "Missing WiiMC original"
    assert hashlib.sha256(original.read_bytes()).hexdigest() == backup, "WiiMC recovery hash mismatch"
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
    assert isinstance(manager["channel_wad_included"], bool), "invalid WAD inventory"
    if manager["channel_wad_included"]:
        channel = manifest.get("channel")
        assert isinstance(channel, dict), "channel provenance missing"
        assert channel.get("title_id") == "MIZN", "wrong channel title ID"
        assert channel.get("ios") == 58, "unexpected channel IOS"
        assert channel.get("real_wii_verified") is False, "unverified Wii claim"
        rel = channel.get("path")
        assert rel == "wad/Maizo-Frontend-MIZN-SD-USB.wad"
        wad = root / rel
        assert wad.is_file() and wad.stat().st_size >= 1024, "missing channel WAD"
        digest=hashlib.sha256(wad.read_bytes()).hexdigest()
        assert digest == channel.get("sha256"), "channel WAD SHA mismatch"
        assert digest == "048df39a5b4bbb4ba77da8ea1fc6066061d77fcdf712199e939aa38ed731e96c", "unapproved MIZN WAD"
    else:
        assert not list((root / "wad").glob("*.wad")), "untracked WAD in bundle"
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
print("PASS: all nine application executables and controller profiles present")
