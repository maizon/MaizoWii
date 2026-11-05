"""Read-only CUE preflight: actual C production implementation compiled on host."""
import ctypes
from pathlib import Path
import shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class PS1DiscTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which("cc")
        if not cc:raise unittest.SkipTest("host compiler missing")
        cls.tmp=tempfile.TemporaryDirectory()
        lib=Path(cls.tmp.name)/"libps1.so"
        subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-shared",
                        "-fPIC","-I",str(ROOT/"include"),
                        str(ROOT/"source/ps1_cue.c"),"-o",str(lib)],
                       check=True,capture_output=True,text=True)
        cls.fn=ctypes.CDLL(str(lib)).maizo_probe_ps1_image
        cls.fn.argtypes=(ctypes.c_char_p,ctypes.c_void_p,ctypes.c_size_t)
        cls.fn.restype=ctypes.c_int
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def probe(self,path):
        error=ctypes.create_string_buffer(180)
        r=self.fn(str(path).encode(),error,len(error))
        return r,error.value.decode(errors="replace")
    def test_valid_disc_and_case(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            (root/"Track 01.bin").write_bytes(b"0"*256)
            cue=root/"disc.CUE"
            cue.write_text('FILE "Track 01.bin" BINARY\n  TRACK 01 MODE2/2352\n')
            self.assertEqual(self.probe(cue)[0],1)
            self.assertEqual(self.probe(root/"Track 01.bin")[0],1)
    def test_missing_track_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            cue=Path(tmp)/"bad.cue"
            cue.write_text('FILE "missing.bin" BINARY\n')
            ok,error=self.probe(cue)
            self.assertEqual(ok,0)
            self.assertIn("trilha ausente",error)
    def test_empty_and_broken_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); cue=root/"empty.cue"
            cue.write_text("REM empty\n")
            self.assertEqual(self.probe(cue)[0],0)
            self.assertEqual(self.probe(root/"missing.iso")[0],0)
            binary=root/"empty.iso";binary.write_bytes(b"123")
            self.assertEqual(self.probe(binary)[0],0)
