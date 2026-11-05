# MAIZO OSC/FIX94 forwarder v4

This component **replaces** the previous experimental MAIZO MEM2 forwarder.
It ports the **working Wii forwarder implementation** distributed as
[OpenShopChannel/osc-forwarder](https://github.com/OpenShopChannel/osc-forwarder)
(pinned revision `b90b8aa91469f3b7d2659bcd11ef1929ba3dda65`), which
acknowledges the original forwarder by **The Lemon Man, Dimok, FIX94, oggzee**,
with additional credit to WiiPower, luccax, Aurelio and crediar.

- The original 872-byte `data/app_booter.bin` remains byte-for-byte unchanged.
  Git blob SHA-1: `08d1806350e35c2d4e633b9527bf93dd770a0820`.
- The original notice is preserved in `main.c`. MIT license in
  `LICENSE-OSC-MIT`.
- **MAIZO changes:** exact SD and USB FAT mounts (no `fatInitDefault`),
  bounded USB startup retries, paths `apps/maizo/boot.dol` and
  `apps/MaizoWii/boot.dol`, DOL sanity checks, diagnostics on screen,
  USB/SD `maizo/forwarder.log` on failure. No writes to NAND.
- Boot-stage text uniquely identifies the version:
  **MAIZO OSC/FIX94 FORWARDER v5**. If the user's channel does not show
  that marker, the **old WAD** is still installed.
- The front end itself must be placed at `usb:/apps/maizo/boot.dol`
  or `sd:/apps/maizo/boot.dol`; a WAD alone does NOT install the app.
- A boot via HBC with `apps/maizo-forwarder/boot.dol` can help isolate
  SD/USB mounting from the channel WAD packaging.
- This code and packaging require physical Wii validation. The repository
  **does not instruct automatic WAD installation or NAND modifications**.

IMPORTANT: the WAD is experimental and structural checks do not establish
banner safety. Test in Dolphin's Wii NAND first and have a NAND backup
before considering installation on a real Wii.
