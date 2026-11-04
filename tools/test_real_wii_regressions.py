#!/usr/bin/env python3
"""Regression guards for the real 0x80003F00 DOLs from CI #166."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WiiRealBootRegressions(unittest.TestCase):
    def test_devkitpro_3f00_entry_supported_in_both_loaders(self):
        launch = (ROOT / "source/launch.c").read_text()
        forwarder = (ROOT / "forwarder/main.c").read_text()
        self.assertIn("0x80003F00u", launch)
        self.assertIn("0x80003F00u", forwarder)
        self.assertNotIn("addr<0x80004000", launch)
        self.assertIn("entry_in_text", launch)

    def test_forwarder_diagnostics_and_usb_mount_recovery(self):
        fw = (ROOT / "forwarder/main.c").read_text()
        for marker in ("DOL rejeitado:", "leitura incompleta",
                       "fatMountSimple(\"usb\"",
                       "sd:/apps/maizo/boot.dol", "usb:/apps/maizo/boot.dol"):
            self.assertIn(marker, fw)

    def test_bootstub_clears_header_bss(self):
        stub = (ROOT / "bootstub/stub.c").read_text()
        self.assertIn("clear_bytes", stub)
        self.assertIn("if(h->bss_size)clear_bytes", stub)

    def test_forwarder_log_and_bss_cache(self):
        stub=(ROOT/"bootstub/stub.c").read_text()
        fw=(ROOT/"forwarder/main.c").read_text()
        self.assertIn('asm volatile("dcbst',stub)
        self.assertIn('sd:/maizo/forwarder.log',fw)
        self.assertIn('save_forwarder_failure();',fw)

    def test_ps1_fail_closed_and_wiimc_avoids_autoboot_exit(self):
        patch = (ROOT / "tools/patch_upstreams.py").read_text()
        self.assertIn("WiiSXRX exact autoboot filename", patch)
        self.assertIn("Autoboot = false;", patch)
        self.assertIn("WiiMC do not reboot at video EOF", patch)
        self.assertIn('PAD_ControlMotor(0, i);', patch)
        self.assertIn("WiiMC libass aligned allocator prototype", patch)
        self.assertIn("WiiMC propagate MPlayer build failures", patch)
        workflow = (ROOT / ".github/workflows/build-adapted-modules.yml").read_text()
        self.assertIn("rm -f wiimc.dol wiimc.elf", workflow)
        self.assertIn("WiiMC source adaptation did not change the binary", workflow)


if __name__ == "__main__":
    unittest.main()
