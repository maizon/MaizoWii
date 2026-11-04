"""Cache invalidation must be isolated to modified modules and true dependencies."""
from __future__ import annotations

import shutil
import tempfile
import unittest
from pathlib import Path

from component_fingerprint import HID_FILES, ROOT, fingerprint


class ComponentFingerprintTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for path in ("tools/patch_upstreams.py", *HID_FILES):
            (self.root / path).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / path, self.root / path)

    def patch_source(self, before: str, after: str):
        path = self.root / "tools/patch_upstreams.py"
        source = path.read_text(encoding="utf-8")
        self.assertIn(before, source)
        path.write_text(source.replace(before, after, 1), encoding="utf-8")

    def test_wiimc_patch_does_not_rebuild_unchanged_modules(self):
        modules = ("WiiMC", "WiiSXRX", "snes9xgx", "fceugx", "vbagx", "usbloader_gx")
        original = {name: fingerprint(name, self.root) for name in modules}
        marker = "def patch_wiimc(root: Path):"
        self.patch_source(marker, marker + "\n    _signature_test = 1")
        updated = {name: fingerprint(name, self.root) for name in modules}
        self.assertNotEqual(original["WiiMC"], updated["WiiMC"])
        for name in modules:
            if name != "WiiMC":
                self.assertEqual(original[name], updated[name], name)

    def test_common_header_invalidates_every_consumer(self):
        modules = ("WiiMC", "WiiSXRX", "snes9xgx", "fceugx", "vbagx", "usbloader_gx")
        original = {name: fingerprint(name, self.root) for name in modules}
        path = self.root / HID_FILES[1]
        path.write_bytes(path.read_bytes() + b"\n// shared change\n")
        for name in modules:
            self.assertNotEqual(original[name], fingerprint(name, self.root))

    def test_gx_mapping_only_invalidates_gx_modules(self):
        original = {name: fingerprint(name, self.root)
                    for name in ("WiiMC", "fceugx", "vbagx", "snes9xgx")}
        self.patch_source("MAP_GENERIC = '''", "MAP_GENERIC = '''\n/* modified */")
        for name in ("fceugx", "vbagx"):
            self.assertNotEqual(original[name], fingerprint(name, self.root))
        for name in ("WiiMC", "snes9xgx"):
            self.assertEqual(original[name], fingerprint(name, self.root))

    def test_changed_dispatch_map_fails_closed(self):
        self.patch_source('"WiiMC": patch_wiimc', '"WiiMC": patch_usbloader')
        with self.assertRaises(ValueError):
            fingerprint("WiiMC", self.root)


if __name__ == "__main__":
    unittest.main()
