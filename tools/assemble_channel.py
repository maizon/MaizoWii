#!/usr/bin/env python3
"""Register the user-approved MIZN WAD in the single ALL-IN-ONE package.
This does not install the channel or claim Wii hardware validation.
"""
import hashlib
import json
import pathlib
import sys

def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "dist")
    target = root / "wad" / "Maizo-Frontend-MIZN-SD-USB.wad"
    if not target.is_file() or target.stat().st_size < 1024:
        raise SystemExit("WAD absent or too small; refuse incomplete package")
    manifest_path = root / "maizo" / "MODULE_MANIFEST.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    manager = manifest.get("installer")
    if not manager or manager.get("auto_installs") is not False:
        raise SystemExit("Interactive WAD installer provenance invalid")
    manager["channel_wad_included"] = True
    manifest["channel"] = {
        "path": "wad/" + target.name,
        "sha256": digest, "title_id": "MIZN", "ios": 58,
        "dolphin_verified": False, "real_wii_verified": False,
        "status": "experimental - structure only",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (root / "wad" / "SHA256SUMS").write_text(digest + "  " + target.name + "\n", encoding="ascii")
    (root / "wad" / "LEIA-ANTES-DE-INSTALAR.txt").write_text(
        "MAIZOWII MIZN - CANAL FORWARDER EXPERIMENTAL.\n"
        "Este WAD pode alterar a NAND e causar brick. NAO instale sem\n"
        "backup da NAND, BootMii/Priiloader recuperavel e testes no Dolphin.\n"
        "Teste PRIMEIRO apps/maizo-forwarder/boot.dol pelo Homebrew Channel.\n"
        "O canal deve abrir usb:/apps/maizo/boot.dol ou sd:/apps/maizo/boot.dol.\n"
        "Instalacao SOMENTE manual e confirmada pelo usuario no YAWM.\n"
        "Compilar e inspecionar um WAD NAO prova funcionamento no Wii real.\n",
        encoding="utf-8",
    )
    (root / "maizo" / "LEIA-PRIMEIRO.txt").write_text(
        "Copie o CONTEUDO deste ZIP para a raiz FAT32 do USB ou SD.\n"
        "Abra apps/maizo/boot.dol pelo Homebrew Channel antes de testar o canal.\n"
        "O WAD em wad/ e EXPERIMENTAL, com risco de brick; leia o aviso.\n"
        "Os nucleos HID nao sao garantidos em todos os controles.\n",
        encoding="utf-8",
    )
    print("PASS channel inventory registered: " + digest)

if __name__ == "__main__":
    main()
