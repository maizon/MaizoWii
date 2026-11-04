#!/usr/bin/env python3
"""Non-installing outer Wii WAD sanity checker.

This is not proof against banner bricks. Structural checks cannot replace
loading the package in Dolphin's Wii System Menu or a genuine Wii.
"""
from pathlib import Path
import hashlib,struct,sys

def align64(value):return (value+63)&~63
def verify(path):
    data=Path(path).read_bytes()
    if len(data)<0x400 or len(data)>32*1024*1024:
        raise AssertionError("Invalid WAD size")
    head,kind,cert,crl,tik,tmd,content,footer=struct.unpack_from(">8I",data)
    assert head==0x20, f"Unexpected WAD header: {head}"
    assert kind in (0x49730000,0x69620000),f"Bad WAD type: {kind:x}"
    assert 0x300<=cert<0x10000,"Invalid cert chain"
    assert crl<0x100000,"Invalid CRL"
    assert 0x250<=tik<0x1000,"Invalid ticket"
    assert 0x1e4<=tmd<0x10000,"Invalid TMD"
    assert 0<content<32*1024*1024,"Invalid encrypted data"
    offset=align64(head)
    offsets={}
    for name,size in (("cert",cert),("crl",crl),("tik",tik),
                      ("tmd",tmd),("data",content),("footer",footer)):
        offsets[name]=(offset,size)
        assert offset+size<=len(data),f"Truncated {name}"
        offset=align64(offset+size)
    assert offset-len(data)<64,"Unexpected WAD trailing payload"
    # For RSA-2048 Wii tickets/TMDs, title ID is at 0x1dc in ticket
    # and 0x18c in TMD. Their agreement prevents accidental title overwrite.
    ticket=data[offsets["tik"][0]:offsets["tik"][0]+tik]
    title=data[offsets["tmd"][0]:offsets["tmd"][0]+tmd]
    expected=bytes.fromhex("000100014D5A494F") # 00010001-MZIO
    assert ticket[0x1dc:0x1e4]==expected, "Wrong ticket Title ID"
    assert title[0x18c:0x194]==expected, "Wrong TMD Title ID"
    # IOS58 is 0x00000001-0000003a. This must NOT overwrite it.
    assert title[0x184:0x18c]==bytes.fromhex("000000010000003A"),"Startup IOS not 58"
    total_titles=struct.unpack_from(">H",title,0x1de)[0]
    boot_index=struct.unpack_from(">H",title,0x1e0)[0]
    assert total_titles in (2,3),f"Unexpected number of contents {total_titles}"
    assert boot_index in range(total_titles),"Invalid boot index"
    print(f"PASS WAD header, sections, title 00010001-MZIO, IOS58, {total_titles} contents, boot={boot_index}")
    print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")
    print("NOT A GUARANTEE: test the channel in a Dolphin Wii NAND before installing.")

if __name__=="__main__":
    verify(sys.argv[1])
