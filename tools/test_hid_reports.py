#!/usr/bin/env python3
"""Build the production portable HID descriptor parser with host GCC.

These tests establish bit/length/report-ID handling, NOT Wii USB/IOS support.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r"""
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "maizo_report.h"
static const uint8_t pad[]={
 0x05,0x01,0x09,0x05,0xa1,0x01,
 0x05,0x09,0x19,0x01,0x29,0x08,0x15,0x00,0x25,0x01,
 0x95,0x08,0x75,0x01,0x81,0x02,
 0x05,0x01,0x09,0x30,0x09,0x31,
 0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x02,0x81,0x02,
 0x09,0x39,0x15,0x00,0x25,0x07,0x75,0x04,0x95,0x01,0x81,0x02,
 0x75,0x04,0x95,0x01,0x81,0x03,0xc0
};
int main(void){
 MzrMap m;MzrState s;
 assert(mzr_parse(pad,sizeof(pad),&m));
 uint8_t a[]={0x05,0x00,0xff,0x02};
 assert(mzr_decode(&m,a,sizeof(a),&s));
 assert(s.buttons==5 && s.x==-96 && s.y==96);
 assert(s.right && !s.left && !s.down && !s.up);
 assert(!mzr_decode(&m,a,1,&s)); /* truncated */
 a[3]=8;assert(mzr_decode(&m,a,sizeof(a),&s));
 assert(!(s.up||s.down||s.left||s.right)); /* neutral 8 */
 a[3]=0x0f;assert(mzr_decode(&m,a,sizeof(a),&s));
 assert(!(s.up||s.down||s.left||s.right)); /* neutral 0xf */
 assert(!mzr_parse(pad,sizeof(pad)-1,&m)); /* unterminated collection */
 uint8_t bad[]={0x05,0x01,0x09,0x02,0xa1,0x01,0xc0};
 assert(!mzr_parse(bad,sizeof(bad),&m)); /* mouse rejected */
 uint8_t trunc[]={0x05,0x01,0x09,0x05,0xa1};
 assert(!mzr_parse(trunc,sizeof(trunc),&m));
 uint8_t long_bad[]={0xfe,0xff,0x00};
 assert(!mzr_parse(long_bad,sizeof(long_bad),&m));
 /* Explicit report ID 2 and byte offset */
 uint8_t numbered[sizeof(pad)+2];
 memcpy(numbered,pad,6);numbered[6]=0x85;numbered[7]=2;
 memcpy(numbered+8,pad+6,sizeof(pad)-6);
 assert(mzr_parse(numbered,sizeof(numbered),&m));
 assert(m.has_id);
 uint8_t id2[]={2,0x03,128,128,0x01};
 assert(mzr_decode(&m,id2,sizeof(id2),&s));
 assert(s.buttons==3 && s.right);
 id2[0]=3;assert(!mzr_decode(&m,id2,sizeof(id2),&s));
 /* Direct bit-extraction tests */
 uint32_t value=0;
 const uint8_t bits[]={0x80,0x01};
 assert(mzr_bits(bits,2,7,2,&value)&&value==3);
 assert(!mzr_bits(bits,2,15,2,&value));
 assert(!mzr_bits(bits,2,0,17,&value));
 assert(mzr_scale(-127,-127,127)==-96);
 assert(mzr_scale(127,-127,127)==96);
 assert(mzr_scale(0,0,0)==0);
 return 0;
}
"""

class HidDescriptorContracts(unittest.TestCase):
    def test_gamepad_descriptor_parser(self):
        gcc = shutil.which("cc")
        if gcc is None:
            self.skipTest("C compiler absent")
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            source = directory / "main.c"
            binary = directory / "hid-test"
            source.write_text(HARNESS, encoding="utf-8")
            subprocess.run(
                [gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "integrations"),
                 str(source), "-o", str(binary)], check=True,
                capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True)

if __name__ == "__main__":
    unittest.main()
