#!/usr/bin/env python3
"""Verify WiiMC 3.0 ORIGINAL codec archives: no FFmpeg/MPlayer rebuild.

The packed *.a objects are the proven pinned upstream playback engine.
The Maizo adapter compiles only the application UI/input integration with HID.
A green compile is not a substitute for real Wii playback.
"""
import hashlib
import sys
from pathlib import Path

ORIGINAL_WIIMC_3_LIBRARIES = {
    "source/mplayer/libmplayerwii.a": "30005a0393abc9b121c9a07c1678c9d2e313f1fd",
    "source/mplayer/ffmpeg/libavformat/libavformat.a": "e5fa824172c337dede37b3d4aec73ebe18372f14",
    "source/mplayer/ffmpeg/libavcodec/libavcodec.a": "034eab5148ac113847f987e347b069118c25d229",
    "source/mplayer/ffmpeg/libswscale/libswscale.a": "65332972e4bd95d1c89bb526e879c2a3a8b01605",
    "source/mplayer/ffmpeg/libavutil/libavutil.a": "fa23b26b01101632bd12848205f565c2df847abc",
}
def blob_sha1(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

def check_engine(root: Path) -> None:
    for name, expected in ORIGINAL_WIIMC_3_LIBRARIES.items():
        path = root / name
        if not path.is_file():
            raise ValueError(f"Original WiiMC 3.0 archive missing: {name}")
        data = path.read_bytes()
        actual = blob_sha1(data)
        if actual != expected:
            raise ValueError(f"Original WiiMC 3.0 archive changed: {name}: {actual}")
        print(f"ORIGINAL-WIIMC-3-CODEC {name} bytes={len(data)} sha1={actual}")
    print("PASS: ORIGINAL WiiMC 3.0 codec engine preserved byte-for-byte.")

if __name__ == "__main__":
    check_engine(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("upstream"))
