#!/usr/bin/env python3
"""Turn an externally built PPC freestanding bootstrap into a C byte array."""
import sys
from pathlib import Path
payload=Path(sys.argv[1]).read_bytes()
if not payload or len(payload)>65536:
    raise SystemExit("bootstub invalid length")
text="#include <stddef.h>\nconst unsigned char maizo_bootstub[] __attribute__((aligned(32))) = {\n"
for i in range(0,len(payload),20):
    text += " "+",".join("0x%02X"%v for v in payload[i:i+20])+",\n"
text+="};\nconst unsigned maizo_bootstub_len = %d;\n"%len(payload)
Path(sys.argv[2]).write_text(text)
