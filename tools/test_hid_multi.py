#!/usr/bin/env python3
"""Run the production MultiIn parser as C11 on host.
This validates splitter packet boundaries only, not USB/IOS operation.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CODE = r'''
#include <assert.h>
#include <stdint.h>
#include "maizo_multi.h"
int main(void){
 MaizoReportRouting r;
 const uint8_t p[]={1,0x42,0x11,0x22,0x33,0x44,0,0};
 assert(maizo_route_reports(0,0,p,sizeof(p),&r));
 assert(r.count==1 && r.slice[0].port==0 && r.slice[0].length==8);
 assert(maizo_route_reports(1,1,p,sizeof(p),&r));
 assert(!maizo_route_reports(1,2,p,sizeof(p),&r));
 assert(maizo_route_reports(2,1,p,sizeof(p),&r));
 assert(r.count==1 && r.slice[0].port==0);
 const uint8_t pad3[]={3,4};
 assert(maizo_route_reports(2,1,pad3,sizeof(pad3),&r));
 assert(r.slice[0].port==2);
 assert(!maizo_route_reports(2,4,pad3,sizeof(pad3),&r));
 assert(maizo_route_reports(4,2,pad3,sizeof(pad3),&r)==0);
 const uint8_t pad2[]={2,7,8,9};
 assert(maizo_route_reports(4,2,pad2,sizeof(pad2),&r));
 assert(r.slice[0].port==1 && r.slice[0].length==4);
 assert(!maizo_route_reports(4,1,pad2,sizeof(pad2),&r));
 const uint8_t combined[]={10,11,20,21,30,31,40,41};
 assert(maizo_route_reports(3,2,combined,sizeof(combined),&r));
 assert(r.count==4);
 for(int i=0;i<4;i++){
  assert(r.slice[i].port==i);
  assert(r.slice[i].offset==(unsigned)i*2);
  assert(r.slice[i].length==2);
  assert(combined[r.slice[i].offset]==10+i*10);
 }
 assert(maizo_route_reports(3,3,combined,sizeof(combined),&r));
 assert(r.count==2 && r.slice[1].offset==3 && r.slice[1].length==3);
 assert(maizo_route_reports(3,1,combined,sizeof(combined),&r));
 assert(r.count==4 && r.slice[3].offset==3);
 assert(maizo_route_reports(2,0,pad2,sizeof(pad2),&r));
 assert(r.slice[0].port==1);
 assert(!maizo_route_reports(3,0,combined,sizeof(combined),&r));
 assert(!maizo_route_reports(6,0,p,sizeof(p),&r));
 assert(!maizo_route_reports(0,0,p,0,&r));
 assert(!maizo_route_reports(0,0,0,sizeof(p),&r));
 assert(!maizo_route_reports(0,0,p,sizeof(p),0));
 const uint8_t huge[129]={0};
 assert(!maizo_route_reports(0,0,huge,sizeof(huge),&r));
 return 0;
}
'''

class MultiInPacketTests(unittest.TestCase):
    def test_production_routing(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("Host C compiler missing")
        with tempfile.TemporaryDirectory() as path:
            tmp = Path(path)
            c = tmp / "route.c"
            exe = tmp / "route"
            c.write_text(CODE)
            subprocess.run(
                [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "integrations"), str(c), "-o", str(exe)],
                check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True)
if __name__=="__main__":
    unittest.main()
