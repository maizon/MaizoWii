#!/usr/bin/env python3
"""Termux bridge: connects to Wii, prints real hardware responses.
No arbitrary shell execution; only Wii's fixed diagnostic commands.
"""
import argparse
import socket
import time

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--wii", required=True, help="Wii IP shown on television")
    p.add_argument("--port", type=int, default=17446)
    p.add_argument("--interval", type=float, default=2)
    args = p.parse_args()
    while True:
        try:
            with socket.create_connection((args.wii, args.port), timeout=5) as sock:
                sock.settimeout(5)
                print("CONNECTED", sock.recv(512).decode(errors="replace").strip(), flush=True)
                while True:
                    for cmd in ("PING", "STATUS", "MEM"):
                        sock.sendall((cmd+"\n").encode())
                        print(cmd, "=>", sock.recv(512).decode(errors="replace").strip(), flush=True)
                    time.sleep(args.interval)
        except (OSError, KeyboardInterrupt) as exc:
            if isinstance(exc, KeyboardInterrupt):
                break
            print("DISCONNECTED", exc, "retrying...", flush=True)
            time.sleep(3)

if __name__ == "__main__":
    main()
