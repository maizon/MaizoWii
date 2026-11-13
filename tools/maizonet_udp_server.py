#!/usr/bin/env python3
"""Read-only GameCube ISO UDP sector server for MaizoNet (Android/Termux).
Only run on a trusted LAN. No root, Raspberry Pi or full ISO download.
Wii config: protocol=udp, udp_port=17445, existing SMB1 setup retained
for its initial 128KiB header; this service streams remaining disc blocks.
"""
import argparse
import os
from pathlib import Path
import secrets
import socket
import struct
import time
import zlib

REQ = struct.Struct("!4sBBHIIQH")   # 26 bytes
RES = struct.Struct("!4sBBBBIIQHHHHIQ")  # 44 bytes
PAYLOAD = 1024
BLOCK = 8192
TTL = 600
# Each 8-KiB read generates eight UDP packets. The IOS Wii receive queue
# may drop a back-to-back burst; pace pairs of packets very slightly.
# Adjustable to zero to test throughput on a wired/low-loss network.
DEFAULT_PAIR_PACING_US = 300

class BlockServer:
    def __init__(self, root, pair_pacing_us=DEFAULT_PAIR_PACING_US):
        self.root = Path(root).resolve(strict=True)
        self.pair_pacing_us = pair_pacing_us
        self.clients = {}  # (peer IP, peer UDP port, token) => (fd, size, expires)
        self.bytes_served = 0

    def _reply(self, sock, peer, op, req, token=0, offset=0,
               status=0, index=0, count=0, part=b"", start=0, size=0):
        packet = RES.pack(b"MZUR", 1, op, status, 0, req, token, offset,
                          index, count, len(part), start,
                          zlib.crc32(part) & 0xffffffff, size)
        sock.sendto(packet + part, peer)

    def handle(self, sock, packet, peer):
        if len(packet) < REQ.size:
            return
        magic, ver, op, _, req, token, offset, size = REQ.unpack_from(packet)
        if magic != b"MZUQ" or ver != 1:
            return
        if op == 1:
            try:
                path = packet[REQ.size:].decode("utf-8").replace("\\", "/")
                parts = Path(path)
                if not path or parts.is_absolute() or any(p in (".", "..") for p in parts.parts):
                    raise ValueError("invalid path")
                target = (self.root / parts).resolve(strict=True)
                if not target.is_relative_to(self.root) or target.suffix.lower() not in (".iso", ".gcm"):
                    raise ValueError("not an ISO inside the configured library")
                # Only regular files. Never send directory listings or other files.
                if not target.is_file():
                    raise ValueError("not a regular file")
                fd = os.open(target, os.O_RDONLY)
                filesize = os.fstat(fd).st_size
                if filesize < 0x20000:
                    os.close(fd)
                    raise ValueError("ISO is too short")
                if len(self.clients) >= 32:
                    # Bound resource usage on Android.
                    k = next(iter(self.clients))
                    os.close(self.clients.pop(k)[0])
                tok = secrets.randbits(32) or 1
                self.clients[(peer[0], peer[1], tok)] = (fd, filesize, time.monotonic() + TTL)
                self._reply(sock, peer, 1, req, token=tok, size=filesize)
            except (OSError, ValueError, UnicodeError) as exc:
                self._reply(sock, peer, 1, req, status=1)
            return
        key = (peer[0], peer[1], token)
        item = self.clients.get(key)
        if item is None or item[2] < time.monotonic():
            if item:
                os.close(self.clients.pop(key)[0])
            self._reply(sock, peer, op, req, token=token, offset=offset, status=2)
            return
        fd, filesize, _ = item
        if op == 3:
            os.close(self.clients.pop(key)[0])
            return
        if op != 2 or not 1 <= size <= BLOCK or offset > filesize or size > filesize - offset:
            self._reply(sock, peer, op, req, token=token, offset=offset, status=3)
            return
        try:
            data = os.pread(fd, size, offset)
            if len(data) != size:
                raise OSError("short read")
            count = (len(data) + PAYLOAD - 1) // PAYLOAD
            for i in range(count):
                if i and (i % 2 == 0) and self.pair_pacing_us:
                    time.sleep(self.pair_pacing_us / 1_000_000)
                start = i * PAYLOAD
                self._reply(sock, peer, 2, req, token=token, offset=offset,
                            index=i, count=count, part=data[start:start + PAYLOAD],
                            start=start)
            self.bytes_served += len(data)
            self.clients[key] = (fd, filesize, time.monotonic() + TTL)
        except OSError:
            self._reply(sock, peer, 2, req, token=token, offset=offset, status=4)

def main():
    parser = argparse.ArgumentParser(description="MaizoNet UDP read-only ISO sector server")
    parser.add_argument("--root", default="/storage/emulated/0/PS2")
    parser.add_argument("--port", type=int, default=17445)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--pair-pacing-us", type=int, default=DEFAULT_PAIR_PACING_US,
                        help="Delay after each pair of UDP fragments (default: 300 us)")
    args = parser.parse_args()
    if not 0 <= args.pair_pacing_us <= 5000:
        parser.error("--pair-pacing-us must be 0..5000")
    if not 1024 <= args.port <= 65535:
        parser.error("port must be 1024..65535")
    server = BlockServer(args.root, args.pair_pacing_us)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    print(f"MaizoNet UDP: read-only {server.root} on {args.bind}:{args.port}", flush=True)
    print("LAN only. Keep Termux running while playing GameCube.", flush=True)
    while True:
        packet, peer = sock.recvfrom(2048)
        server.handle(sock, packet, peer)

if __name__ == "__main__":
    main()
