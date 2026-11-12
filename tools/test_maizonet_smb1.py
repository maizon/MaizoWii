#!/usr/bin/env python3
"""Host-compiled wire-level SMB1 regression; no real Samba/Wii validation."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class SMB1WireTests(unittest.TestCase):
    def test_ios_hostid_signed_ipv4(self):
        """A 192.168.x.x address is a valid IPv4 even though signed negative."""
        cc = shutil.which("cc") or shutil.which("gcc")
        self.assertIsNotNone(cc)
        with tempfile.TemporaryDirectory() as tmp:
            binary = pathlib.Path(tmp) / "ipv4-hostid-test"
            build = subprocess.run([
                cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I" + str(ROOT / "integrations"),
                str(ROOT / "tests" / "maizonet_hostid.c"),
                "-o", str(binary)
            ], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([binary], check=True, capture_output=True, text=True)
            self.assertIn("PASS", result.stdout)

    def test_smb1_guest_random_read(self):
        cc = shutil.which("cc") or shutil.which("gcc")
        self.assertIsNotNone(cc)
        with tempfile.TemporaryDirectory() as tmp:
            binary = pathlib.Path(tmp) / "smb1-test"
            build = subprocess.run([
                cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I" + str(ROOT / "integrations"),
                str(ROOT / "integrations" / "maizonet_smb1.c"),
                str(ROOT / "tests" / "smb1_wire.c"),
                "-o", str(binary)
            ], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([binary], check=True, capture_output=True, text=True)
            self.assertIn("PASS", result.stdout)

if __name__ == "__main__":
    unittest.main()
