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
        self.assertIn("MAIZO OSC/FIX94 FORWARDER v4",source)
        self.assertIn("osc_app_booter",source)
        self.assertNotIn("maizo_bootstub",source)
        self.assertIn("SYS_ResetSystem(SYS_SHUTDOWN",source)
        self.assertNotIn("__exception_closeall();",source)
        self.assertIn("SYS_SHUTDOWN already tears down libogc",source)
        self.assertIn('usb:/apps/maizo/boot.dol',source)
        self.assertIn('sd:/apps/maizo/boot.dol',source)
        self.assertIn('fatMountSimple(name,disk)',source)
        self.assertNotIn("fatInitDefault(",source.replace(
            "/* Unlike fatInitDefault(),","/* Unlike default init"))
    def test_wad_uses_explicit_replacement_name(self):
        w=(ROOT/".github/workflows/forwarder-wad.yml").read_text()
        self.assertIn("MAIZOWII-FORWARDER-OSC-FIX94-v4-MZIO-EXPERIMENTAL.wad",w)
        self.assertIn("-id MZIO",w)
        self.assertIn("-ios 58",w)
        self.assertIn("MAIZO OSC/FIX94 FORWARDER v4",w)

if __name__=="__main__":
    unittest.main()
