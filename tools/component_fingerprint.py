#!/usr/bin/env python3
"""Content-addressed build signature for each *independent* Maizo Wii adapter.

Fingerprints the selected upstream patch function and the common helpers it
actually uses. An edit to patch_wiimc must not invalidate WiiSXRX/SNES caches.
Shared HID headers correctly invalidate every adapter that includes them.
Toolchain image, upstream commit and module identity are added by CI.
"""
from __future__ import annotations

import argparse
import ast
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH_FUNCTIONS = {
    "WiiMC": "patch_wiimc",
    "WiiSXRX": "patch_wiisx",
    "WiiSX": "patch_wiisx422",
    "usbloader_gx": "patch_usbloader",
    "snes9xgx": "patch_snes",
    "fceugx": "patch_gx",
    "vbagx": "patch_gx",
}
COMMON = ("replace_once", "patch_file", "install_inc")
USES_PAD_UPDATE = frozenset(("WiiMC", "usbloader_gx"))
GX = frozenset(("fceugx", "vbagx"))
HID_FILES = (
    "integrations/maizo_hid.inc",
    "integrations/maizo_report.h",
    "integrations/maizo_multi.h",
)


def fingerprint(module: str, root: Path = ROOT) -> str:
    if module not in PATCH_FUNCTIONS:
        raise ValueError("Unknown module " + module)
    source = (root / "tools/patch_upstreams.py").read_text(encoding="utf-8")
    tree = ast.parse(source, filename="tools/patch_upstreams.py")
    functions = {
        node.name: node for node in tree.body
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
    }
    mapping = next((
        node.value for node in tree.body if isinstance(node, ast.Assign)
        and any(isinstance(t, ast.Name) and t.id == "MODULES"
                for t in node.targets)
    ), None)
    if not isinstance(mapping, ast.Dict):
        raise ValueError("Expected static MODULES mapping in patch_upstreams.py")
    actual_mapping = {}
    for k, v in zip(mapping.keys, mapping.values):
        if not isinstance(k, ast.Constant) or not isinstance(v, ast.Name):
            raise ValueError("Non-static adapter mapping; update fingerprint tool")
        actual_mapping[k.value] = v.id
    if actual_mapping != PATCH_FUNCTIONS:
        raise ValueError("Adapter mapping changed; update fingerprint tool")

    selected = set(COMMON) | {PATCH_FUNCTIONS[module]}
    if module in USES_PAD_UPDATE:
        selected.add("fix_pad_update")
    missing = selected - set(functions)
    if missing:
        raise ValueError("Missing patch helpers: " + repr(sorted(missing)))

    digest = hashlib.sha256()
    def update(label: str, value: bytes) -> None:
        label_bytes = label.encode("utf-8")
        digest.update(len(label_bytes).to_bytes(4, "big"))
        digest.update(label_bytes)
        digest.update(len(value).to_bytes(8, "big"))
        digest.update(value)

    update("format", b"maizo-adapter-cache-v2")
    update("module", module.encode("ascii"))
    for node in tree.body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            update("import", ast.dump(node, include_attributes=False).encode())
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            names = set()
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for target in targets:
                names.update(n.id for n in ast.walk(target) if isinstance(n, ast.Name))
            if "MODULES" in names:
                continue  # mapping is explicitly verified above
            if "MAP_GENERIC" in names and module not in GX:
                continue
            update("global", ast.dump(node, include_attributes=False).encode())

    for name in sorted(selected):
        update("function:" + name,
               ast.dump(functions[name], include_attributes=False).encode())
    for name in HID_FILES:
        update(name, (root / name).read_bytes())
    if module == "WiiMC":
        # Only this adapter is sensitive to a different linked libogc ABI.
        # Isolated cache invalidation forces one genuine rebuild when the
        # WiiMC original codec dependencies are restored.
        update("wiimc-toolchain-abi", b"original-pinned-libogc-and-portlibs-v9-retroarch-syscalls")
        update("wiimc-syscall-bridge", (root / "integrations/wiimc_syscalls_compat.c").read_bytes())
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", choices=sorted(PATCH_FUNCTIONS))
    args = parser.parse_args()
    print(fingerprint(args.module))


if __name__ == "__main__":
    main()
