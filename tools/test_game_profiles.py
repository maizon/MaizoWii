#!/usr/bin/env python3
"""Production C FNV path contract for isolated game profiles."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
CODE=r'''
#include <assert.h>
#include <string.h>
#include "game_profile.h"
int main(void) {
 char a[128],b[128],c[128],d[128];
 assert(maizo_game_profile_path(1,"sd:/games/Alpha/game.iso",a,sizeof(a)));
 assert(strstr(a,"sd:/maizo/game-profiles/")==a);
 assert(strstr(a,".ini")!=0);
 assert(maizo_game_profile_path(1,"sd:/games/Alpha/game.iso",b,sizeof(b)));
 assert(strcmp(a,b)==0);
 assert(maizo_game_profile_path(2,"sd:/games/Alpha/game.iso",c,sizeof(c)));
 assert(strcmp(a,c)!=0);
 assert(maizo_game_profile_path(1,"usb:/games/Alpha/game.iso",d,sizeof(d)));
 assert(strstr(d,"usb:/maizo/game-profiles/")==d);
 assert(strcmp(a,d)!=0);
 assert(!maizo_game_profile_path(1,"../../../../wad",c,sizeof(c)));
 assert(!maizo_game_profile_path(1,"sd:/foo",c,20));
 assert(!maizo_game_profile_path(1,0,c,sizeof(c)));
 assert(!maizo_game_profile_path(1,"sd:/foo",0,sizeof(c)));
 return 0;
}
'''
class GameProfilePaths(unittest.TestCase):
    def test_profile_names_are_stable_and_device_bounded(self):
        cc=shutil.which("cc")
        if cc is None:self.skipTest("No host C compiler")
        with tempfile.TemporaryDirectory() as td:
            source=Path(td)/"profile.c"
            exe=Path(td)/"profile"
            source.write_text(CODE)
            subprocess.run([cc,"-std=c11","-O2","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"include"),str(source),"-o",str(exe)],
                capture_output=True,text=True,check=True)
            subprocess.run([str(exe)],check=True,capture_output=True)
if __name__=="__main__": unittest.main()
