#!/usr/bin/env python3
"""Full protocol integration test: C Wii-portable client <-> Python server."""
import os
import select
import threading
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest
ROOT = Path(__file__).resolve().parents[1]


class LossyProxy:
    """Deterministically drop fragment #1 of EVERY 8-KiB response.

    The 8-KiB request can never succeed; the portable Wii client must fall
    back to 2-KiB sector requests. Inject a stale/invalid datagram too, so an
    unrelated packet cannot prematurely abort reception.
    """
    def __init__(self, server_port):
        self.front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.front.bind(("127.0.0.1", 0))
        self.back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.back.bind(("127.0.0.1", 0))
        self.server_port = server_port
        self.client = None
        self.request_sizes = {}
        self.counts = {}
        self.dropped = 0
        self.injected = False
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.stop_event.set()
        self.thread.join(timeout=2)
        self.front.close()
        self.back.close()

    @property
    def port(self):
        return self.front.getsockname()[1]

    def _run(self):
        while not self.stop_event.is_set():
            ready, _, _ = select.select([self.front, self.back], [], [], .08)
            for sock in ready:
                packet, peer = sock.recvfrom(4096)
                if sock is self.front:
                    self.client = peer
                    if len(packet) >= 26 and packet[:4] == b"MZUQ" and packet[5] == 2:
                        request = int.from_bytes(packet[8:12], "big")
                        nbytes = int.from_bytes(packet[24:26], "big")
                        self.request_sizes[request] = nbytes
                        self.counts[nbytes] = self.counts.get(nbytes, 0) + 1
                    self.back.sendto(packet, ("127.0.0.1", self.server_port))
                else:
                    if not self.client:
                        continue
                    if len(packet) >= 44 and packet[:4] == b"MZUR" and packet[5] == 2:
                        request = int.from_bytes(packet[8:12], "big")
                        fragment = int.from_bytes(packet[24:26], "big")
                        if self.request_sizes.get(request) == 8192 and fragment == 1:
                            self.dropped += 1
                            continue
                        if not self.injected:
                            self.front.sendto(b"stale-unrelated-packet", self.client)
                            self.injected = True
                    self.front.sendto(packet, self.client)

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
                with LossyProxy(port) as proxy:
                    lossy = subprocess.run([binary, str(proxy.port)], capture_output=True,
                                           text=True, timeout=30)
                    self.assertEqual(lossy.returncode, 0, lossy.stdout + lossy.stderr)
                    self.assertIn("PASS UDP", lossy.stdout)
                    self.assertTrue(proxy.injected, "test must inject a stale packet")
                    self.assertGreaterEqual(proxy.dropped, 4, "8KiB packets must be lost")
                    self.assertGreaterEqual(proxy.counts.get(2048, 0), 4,
                                            "client must fall back to 2KiB requests")
            finally:
                server.terminate()
                try:
                    server.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.communicate()

if __name__ == "__main__":
    unittest.main()
