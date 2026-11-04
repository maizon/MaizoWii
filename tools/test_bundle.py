#!/usr/bin/env python3
"""No network: synthetic, strictly-formed DOL and ZIP security tests."""
import struct
import tempfile
import unittest
from pathlib import Path
from package_all import valid_dol, choose_dol, safe_extract

def fake_dol():
    words = [0] * 57
    words[0] = 0x100
    words[18] = 0x80004000
    words[36] = 16
    words[56] = 0x80004000
    return struct.pack(">57I", *words) + bytes(0x100 - 57 * 4) + b"\x00" * 16

class BundleTests(unittest.TestCase):
    def test_dol_valid(self):
        self.assertTrue(valid_dol(fake_dol()))
        self.assertFalse(valid_dol(b"junk" * 64))
        self.assertFalse(valid_dol(fake_dol()[:-1]))

    def test_choose_homebrew_not_forwarder(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            (p / "apps/WiiMC").mkdir(parents=True)
            (p / "apps/WiiMC/boot.dol").write_bytes(fake_dol())
            (p / "channel-forwarder").mkdir()
            (p / "channel-forwarder/boot.dol").write_bytes(fake_dol())
            self.assertEqual(choose_dol(p, "WiiMC").parent.name, "WiiMC")

    def test_choose_skips_invalid_preferred_dol(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            (p / "apps/WiiMC").mkdir(parents=True)
            (p / "apps/WiiMC/boot.dol").write_bytes(b"broken")
            (p / "WiiMC").mkdir()
            (p / "WiiMC/boot.dol").write_bytes(fake_dol())
            self.assertEqual(choose_dol(p, "WiiMC"), p / "WiiMC/boot.dol")

    def test_choose_rejects_forwarder_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            (p / "forwarder").mkdir()
            (p / "forwarder/boot.dol").write_bytes(fake_dol())
            with self.assertRaises(ValueError):
                choose_dol(p, "WiiMC")

    def test_zip_windows_drive_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "drive.zip", "w") as z:
                z.writestr("C:/outside.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(p / "drive.zip", p / "unpacked")

    def test_zip_backslash_drive_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "drive.zip", "w") as z:
                z.writestr("D:\\outside.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(p / "drive.zip", p / "unpacked")

    def test_zip_symlink_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            info = zipfile.ZipInfo("link.dol")
            info.create_system = 3
            info.external_attr = (0o120777 << 16)
            with zipfile.ZipFile(path / "link.zip", "w") as z:
                z.writestr(info, "outside")
            with self.assertRaises(ValueError):
                safe_extract(path / "link.zip", path / "unpacked")

    def test_zip_absolute_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            with zipfile.ZipFile(path / "absolute.zip", "w") as z:
                z.writestr("/tmp/outside.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(path / "absolute.zip", path / "unpacked")

    def test_zip_duplicate_member_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "dup.zip", "w") as z:
                z.writestr("dir/file.ini", "one")
                z.writestr("dir/FILE.ini", "two")
            with self.assertRaises(ValueError):
                safe_extract(p / "dup.zip", p / "out")
            self.assertFalse((p / "out").exists())

    def test_zip_backslash_normalized(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "path.zip", "w") as z:
                z.writestr(r"apps\Nintendont\boot.dol", fake_dol())
            safe_extract(p / "path.zip", p / "out")
            self.assertEqual((p / "out/apps/Nintendont/boot.dol").read_bytes(), fake_dol())

    def test_zip_rejects_double_backslash_alias(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "badpath.zip", "w") as z:
                z.writestr(r"apps\\Nintendont\\boot.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(p / "badpath.zip", p / "out")
            self.assertFalse((p / "out").exists())

    def test_zip_rejects_unix_special_file(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            info = zipfile.ZipInfo("device")
            info.create_system = 3
            info.external_attr = (0o020666 << 16)
            with zipfile.ZipFile(p / "special.zip", "w") as z:
                z.writestr(info, "data")
            with self.assertRaises(ValueError):
                safe_extract(p / "special.zip", p / "out")

    def test_zip_rejects_file_as_parent_in_both_orders(self):
        import zipfile
        for entries in (("apps", "apps/maizo/boot.dol"),
                        ("apps/maizo/boot.dol", "apps")):
            with self.subTest(entries=entries), tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp)
                with zipfile.ZipFile(p / "conflict.zip", "w") as z:
                    for entry in entries:
                        z.writestr(entry, "payload")
                with self.assertRaises(ValueError):
                    safe_extract(p / "conflict.zip", p / "out")
                self.assertFalse((p / "out").exists())

    def test_zip_rejects_casefold_parent_collision(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "conflict.zip", "w") as z:
                z.writestr("APPS", "file")
                z.writestr("apps/maizo/boot.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(p / "conflict.zip", p / "out")

    def test_zip_rejects_fat32_unsafe_names(self):
        import zipfile
        for name in ("apps/CON.txt", "apps/COM1.ini", "apps/LPT9",
                     "apps/name?.dol", "apps/trailing. ", "apps/aux",
                     "apps/bad:part", "apps/space "):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp)
                with zipfile.ZipFile(p / "bad.zip", "w") as z:
                    z.writestr(name, fake_dol())
                with self.assertRaises(ValueError):
                    safe_extract(p / "bad.zip", p / "out")
                self.assertFalse((p / "out").exists())

    def test_zip_accepts_fat32_safe_names(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "good.zip", "w") as z:
                z.writestr("apps/Maizo/boot.dol", fake_dol())
                z.writestr("apps/Maizo/meta.xml", "metadata")
            safe_extract(p / "good.zip", p / "out")
            self.assertEqual((p / "out/apps/Maizo/boot.dol").read_bytes(), fake_dol())

    def test_zip_rejects_ambiguous_dot_and_empty_components(self):
        import zipfile
        for name in ("apps/./maizo/boot.dol", "apps//maizo/boot.dol",
                     "./boot.dol", "apps/../boot.dol"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp)
                with zipfile.ZipFile(p / "ambiguous.zip", "w") as z:
                    z.writestr(name, fake_dol())
                with self.assertRaises(ValueError):
                    safe_extract(p / "ambiguous.zip", p / "out")
                self.assertFalse((p / "out").exists())

    def test_zip_normal_directory_entry_still_allowed(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            with zipfile.ZipFile(p / "directories.zip", "w") as z:
                z.writestr("apps/maizo/", "")
                z.writestr("apps/maizo/boot.dol", fake_dol())
            safe_extract(p / "directories.zip", p / "out")
            self.assertEqual((p / "out/apps/maizo/boot.dol").read_bytes(), fake_dol())

    def test_zip_traversal_blocked(self):
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            with zipfile.ZipFile(path / "sample.zip", "w") as z:
                z.writestr("../escape.dol", fake_dol())
            with self.assertRaises(ValueError):
                safe_extract(path / "sample.zip", path / "unpacked")

if __name__ == "__main__":
    unittest.main()
