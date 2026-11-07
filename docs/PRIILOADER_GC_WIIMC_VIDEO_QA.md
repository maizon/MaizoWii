# MaizoWii: Priiloader USB, GameCube, 240p and WiiMC — console QA

## Status / limitations
This change is **source implementation only** until tested on a physical Wii.
The MaizoWii repo is not hardware-verified; successfully compiling a WiiMC
DOL does not prove its UI, HID or player functions work. Do not label stable.

## Boot path and USB
Priiloader starts autoboot before it initializes its interactive FAT mounting
thread. A manually launched Installed File may work after Priiloader warmed up
storage. Frontend startup now initializes SD and USB independently and checks
startup, presence and FAT. Visible boot stages narrow down blocking calls.
libogc synchronous calls may still block; this is not a true hard timeout.

Test cold autoboot with SD only, USB only, both, neither, then direct HBC,
then manual Installed File. Record which boot-stage stays on screen. Do not
modify Priiloader IOS, cIOS or NAND based on a source-level hypothesis.

## GameCube
Nintendont reads the binary v0x0B NIN_CFG from argv[1], including direct-boot
flag, relative game path, VI VideoScale and *horizontal* VideoOffset. Failure
writing root nincfg.bin is not grounds to cancel argv direct boot. VideoScale
can enlarge physical output width 640..720; it does NOT freely shrink below
640, nor does it implement independent height calibration. Game compatibility
and actual direct launch still need Wii validation.

## 240p and 480i
The frontend's logical canvas does not persist to another DOL. Adapted
SNES/WiiSX modules read a per-launch record that now includes frontend
baseline plus per-game geometry. SNES 240p uses the VI hardware width/x
instead of moving its saturated 512px GPU quad past its limits. Verify
center and width with the real CRT for native 224p/239p and 480i. CRT
overscan and 50/60 Hz depend on the TV. Previous settings are preserved.

## WiiMC
WiiMC-SSLC lists `.iso` among validVideoExtensions, and supports real DVD
media, subject to console DVD compatibility. DVD-Video ISO images on USB/SD
may depend on actual player demux behavior and require Wii testing; the
frontend now enumerates files under MEDIA .iso independently of Wii/GC .iso.

Up to 35 *numeric* WiiMC-SSLC SWiiSettings choices are exposed in the
Maizo media settings and copied to the adapted WiiMC's runtime from the
launch record only when changed, after settings XML is loaded. WiiMC's full
GUI remains responsible for advanced string fields, SMB/FTP credentials,
and arbitrary remote streams. The official WiiMC fallback does not consume
the Maizo profile. The experimental WiiMC Dolphin smoke failed in prior CI.

## WAD
Only the user-designated `Maizo-Frontend-MIZN-SD-USB.wad` may be the
standard WAD. Do not publish a different experimental MZIO WAD as standard.
The uploaded WAD currently must be imported to the repository to be
included in GitHub Actions packages. Avoid silent NAND installation.
