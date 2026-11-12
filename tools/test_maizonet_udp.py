#!/usr/bin/env python3
"""Full protocol integration test: C Wii-portable client <-> Python server."""
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest
ROOT = Path(__file__).resolve().parents[1]

class UDPStreamTest(unittest.TestCase):
    def test_udp_sector_transport(self):
        cc = shutil.which("cc") or shutil.which("gcc")
        self.assertIsNotNone(cc)
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            game = folder / "Gamecube/games/XIII/game.iso"
            game.parent.mkdir(parents=True)
            payload = bytes((i * 37 + 11) & 255 for i in range(8192 + 3072))
            with game.open("wb") as f:
                f.truncate((1 << 32) + 0x20000)
            fd = os.open(game, os.O_WRONLY)
            try:
                os.pwrite(fd, payload, 0x20000)
                os.pwrite(fd, b"DATA", (1 << 32) + 0x12)
            finally:
                os.close(fd)
            binary = folder / "udp_test"
            build = subprocess.run([
                cc, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT / "integrations"),
                str(ROOT / "integrations" / "maizonet_udp.c"),
                str(ROOT / "tests" / "maizonet_udp_wire.c"),
                "-o", str(binary)
            ], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.bind(("127.0.0.1", 0))
                port = s.getsockname()[1]
            server = subprocess.Popen([
                sys.executable, "-u", str(ROOT / "tools" / "maizonet_udp_server.py"),
                "--bind", "127.0.0.1", "--root", str(folder),
                "--port", str(port)
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                time.sleep(0.25)
                self.assertIsNone(server.poll(), "server exited before test")
                run = subprocess.run([binary, str(port)], capture_output=True,
                                     text=True, timeout=15)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertIn("PASS UDP", run.stdout)
            finally:
                server.terminate()
                try:
                    server.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.communicate()

if __name__ == "__main__":
    unittest.main()
