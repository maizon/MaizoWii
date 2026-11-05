#!/usr/bin/env python3
"""Embed the unmodified Open Shop Channel/FIX94 app_booter.bin in a C source.

Original upstream: OpenShopChannel/osc-forwarder @ b90b8aa91469f3b7d2659bcd11ef1929ba3dda65
Original blob SHA: 08d1806350e35c2d4e633b9527bf93dd770a0820
"""
from pathlib import Path
import hashlib
import sys

PINNED = "08d1806350e35c2d4e633b9527bf93dd770a0820"

def embed(inp, out):
    payload = Path(inp).read_bytes()
    git_blob = b"blob " + str(len(payload)).encode() + b"\x00" + payload
    actual = hashlib.sha1(git_blob).hexdigest()
    if len(payload) != 872 or actual != PINNED:
        raise SystemExit("Unapproved OSC app_booter payload: " + actual)
    rows = []
    for i in range(0, len(payload), 12):
        rows.append(" " + ",".join("0x%02x" % b for b in payload[i:i+12]) + ",")
    Path(out).write_text(
        "/* Unmodified OSC/FIX94 app_booter; see forwarder/LICENSE-OSC-MIT. */\n"
        "__attribute__((aligned(32))) const unsigned char osc_app_booter[]={\n"
        + "\n".join(rows) + "\n};\n"
        + "const unsigned int osc_app_booter_len=sizeof(osc_app_booter);\n",
        encoding="utf-8"
    )
    print("OSC/FIX94 boot code verified:", actual)

if __name__ == "__main__":
    embed(sys.argv[1], sys.argv[2])
