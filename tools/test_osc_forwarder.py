"""OSC FIX94 forwarder source and upstream booter provenance checks."""
import hashlib
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
FORWARDER=ROOT/"forwarder"
PINNED="08d1806350e35c2d4e633b9527bf93dd770a0820"

class OSCForwarderTests(unittest.TestCase):
    def test_upstream_loader_binary_is_unmodified(self):
        blob=(FORWARDER/"data/app_booter.bin").read_bytes()
        self.assertEqual(len(blob),872)
        h=hashlib.sha1(b"blob "+str(len(blob)).encode()+b"\x00"+blob).hexdigest()
        self.assertEqual(h,PINNED)
    def test_channel_and_hbc_share_osc_booter(self):
        source=(FORWARDER/"main.c").read_text()
        self.assertIn("MAIZO FIX94-COMPAT FORWARDER v6",source)
        self.assertIn("osc_app_booter",source)
        self.assertNotIn("maizo_bootstub",source)
        self.assertIn("SYS_ResetSystem(SYS_SHUTDOWN",source)
        self.assertNotIn("__exception_closeall();",source)
        self.assertIn("__lwp_thread_stopmultitasking",source)
        self.assertIn("__io_usbstorage.shutdown()",source)
        self.assertIn("a->argvMagic=ARGV_MAGIC",source)
        self.assertIn("memcpy(a->commandLine,path,path_bytes)",source)
        self.assertLess(source.index('launch_from("usb:/apps/maizo/boot.dol")'),
                        source.index('launch_from("sd:/apps/maizo/boot.dol")'))
        self.assertIn('usb:/apps/maizo/boot.dol',source)
        self.assertIn('sd:/apps/maizo/boot.dol',source)
        self.assertIn('fatMountSimple(name,disk)',source)
        self.assertNotIn("fatInitDefault(",source.replace(
            "/* Unlike fatInitDefault(),","/* Unlike default init"))
    def test_only_approved_mizn_wad_is_allowed(self):
        w=(ROOT/".github/workflows/forwarder-wad.yml").read_text()
        build=(ROOT/".github/workflows/build.yml").read_text()
        inspector=(ROOT/"tools/inspect_forwarder_wad.py").read_text()
        self.assertIn("Maizo-Frontend-MIZN-SD-USB.wad",w)
        self.assertIn("Maizo-Frontend-MIZN-SD-USB.wad",build)
        self.assertNotIn("MZIO-EXPERIMENTAL.wad",w)
        self.assertNotIn("-id MZIO",build)
        self.assertIn("000100014D495A4E",inspector)

if __name__=="__main__":
    unittest.main()
