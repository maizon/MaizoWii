#!/usr/bin/env python3
"""Regression guards for the real 0x80003F00 DOLs from CI #166."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WiiRealBootRegressions(unittest.TestCase):
    def test_422_integration_and_nintendont_native_geometry(self):
        package = (ROOT / "tools/package_all.py").read_text()
        launch = (ROOT / "source/launch.c").read_text()
        settings = (ROOT / "source/settings.c").read_text()
        self.assertIn("Beta_4.2.2", package)
        self.assertIn("resolve_ps1_wiisx", launch)
        self.assertIn("n.VideoScale=", launch)
        self.assertIn("n.VideoOffset=", launch)
        self.assertIn("CALIBRAR TELA MAIZO", settings)

    def test_wad_mounts_usb_independently(self):
        fw = (ROOT / "forwarder/main.c").read_text()
        front = (ROOT / "source/main.c").read_text()
        self.assertIn("IOS_ReloadIOS(58)", fw)
        self.assertIn('fatMountSimple("usb",&__io_usbstorage)', front)
        self.assertIn("__io_usbstorage.shutdown()",fw)
        self.assertIn("a->argvMagic=ARGV_MAGIC",fw)
        self.assertIn('storage_root("usb:/")', front)

    def test_devkitpro_3f00_entry_supported_in_both_loaders(self):
        launch = (ROOT / "source/launch.c").read_text()
        forwarder = (ROOT / "forwarder/main.c").read_text()
        self.assertIn("0x80003F00u", launch)
        self.assertIn("0x80003f00u", forwarder.lower())
        self.assertNotIn("addr<0x80004000", launch)
        self.assertIn("entry_in_text", launch)

    def test_forwarder_diagnostics_and_usb_mount_recovery(self):
        fw = (ROOT / "forwarder/main.c").read_text()
        for marker in ("DOL invalido/entrada sobreposta", "leitura incompleta",
                       'fatMountSimple(name,disk)', 'mount_one("usb",&__io_usbstorage)',
                       "sd:/apps/maizo/boot.dol", "usb:/apps/maizo/boot.dol"):
            self.assertIn(marker, fw)

    def test_bootstub_clears_header_bss(self):
        stub = (ROOT / "bootstub/stub.c").read_text()
        self.assertIn("clear_bytes", stub)
        self.assertIn("if(h->bss_size){", stub)
        self.assertIn("clear_bytes((void *)(uintptr_t)h->bss,h->bss_size)", stub)

    def test_gx_hid_no_wiimote_input_guard(self):
        patch=(ROOT/"tools/patch_upstreams.py").read_text()
        self.assertIn("USBLoader no-Wiimote input guard", patch)
        self.assertIn("if(maizo_remote)memcpy", patch)
        argv=(ROOT/"source/launch_args.c").read_text()
        self.assertIn('!append(out,"-sdmode=1")',argv)

    def test_forwarder_log_and_bss_cache(self):
        stub=(ROOT/"bootstub/stub.c").read_text()
        fw=(ROOT/"forwarder/main.c").read_text()
        self.assertIn('asm volatile("dcbst',stub)
        self.assertIn('"%s/forwarder.log"',fw)
        self.assertIn('save_failure();',fw)

    def test_ps1_fail_closed_and_wiimc_avoids_autoboot_exit(self):
        patch = (ROOT / "tools/patch_upstreams.py").read_text()
        self.assertIn("WiiSXRX exact autoboot filename", patch)
        self.assertIn("Autoboot = false;", patch)
        # Restore original WiiMC's GUI/video/MPlayer while making no-Wiimote
        # HID operation and argv media failures safe.
        self.assertIn("WiiMC no-Wiimote NULL guard", patch)
        self.assertIn("WiiMC refresh live Wiimote pointers safely", patch)
        self.assertIn("WiiMC missing argv media falls back to stock browser", patch)
        self.assertIn('PAD_ControlMotor(0, i);', patch)
        self.assertIn("WiiMC libass aligned allocator prototype", patch)
        self.assertIn("WiiMC ORIGINAL 3.0 media engine retained", patch)
        self.assertNotIn("WiiMC cap dead startup/unsupported codec", patch)
        self.assertNotIn("WiiMC 3.0 HID control during tiled video", patch)
        workflow = (ROOT / ".github/workflows/build-adapted-modules.yml").read_text()
        self.assertIn("rm -f wiimc.dol wiimc.elf", workflow)
        self.assertIn("WiiMC source adaptation did not change the binary", workflow)
        self.assertIn("verify_wiimc_engine.py", workflow)


if __name__ == "__main__":
    unittest.main()
