"""Source-level checks for original 3.0 codec parity and HID architecture.

The real archive SHA-1 check executes in the WiiMC source-build job.
"""
import ast, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class WiiMC3RegressionTests(unittest.TestCase):
    def test_original_engine_manifest(self):
        mod=ast.parse((ROOT/"tools/verify_wiimc_engine.py").read_text())
        values={}
        for stmt in mod.body:
            if isinstance(stmt,ast.Assign) and any(isinstance(t,ast.Name) and
                 t.id=="ORIGINAL_WIIMC_3_LIBRARIES" for t in stmt.targets):
                values=ast.literal_eval(stmt.value)
        self.assertEqual(len(values),5)
        self.assertIn("source/mplayer/libmplayerwii.a",values)
        self.assertIn("source/mplayer/ffmpeg/libavcodec/libavcodec.a",values)
        self.assertTrue(all(len(v)==40 for v in values.values()))
    def test_main_code_keeps_upstream_codec_engine_and_video_gui(self):
        patch=(ROOT/"tools/patch_upstreams.py").read_text()
        self.assertIn("WiiMC ORIGINAL 3.0 media engine retained",patch)
        self.assertIn("WiiMC GUI-only HID USB enumeration",patch)
        self.assertIn("maizo_hid_tick_attached",patch)
        self.assertIn("WiiMC no-Wiimote NULL guard",patch)
        self.assertIn("WiiMC missing argv media falls back to stock browser",patch)
        self.assertIn("WiiMC conservative legacy GUI compiler semantics",patch)
        self.assertNotIn("WiiMC cap dead startup/unsupported codec",patch)
        self.assertNotIn("WiiMC 3.0 HID control during tiled video",patch)
    def test_legacy_mplayer_binary_ABI_is_bridged(self):
        patch=(ROOT/"tools/patch_upstreams.py").read_text()
        self.assertIn("__ctype_ptr__", patch)
        self.assertIn("matching original WiiMC libogc", patch)
        self.assertNotIn("maizo_compat_gettime", patch)
        self.assertNotIn("maizo_compat_dcflush", patch)
        self.assertIn("WiiMC ORIGINAL 3.0 media engine retained",patch)
    def test_ci_refuses_recompiled_audio_video_libraries(self):
        w=(ROOT/".github/workflows/build-adapted-modules.yml").read_text()
        self.assertGreaterEqual(w.count("verify_wiimc_engine.py"),2)
        self.assertIn("rm -f wiimc.dol wiimc.elf",w)
        self.assertIn("('portlibs.zip','libogc.zip')",w)
        self.assertIn("WiiMC's stock MPlayer/FFmpeg",w)
        bridge=(ROOT/"integrations/wiimc_syscalls_compat.c").read_text()
        self.assertIn("__syscalls",bridge)
        self.assertIn("__syscall_sbrk_r",bridge)
        self.assertIn("__syscall_malloc_lock",bridge)
        self.assertIn("ce8f93cc1c3785f1c8d3d05491d7a899cdcb5479",bridge)
        patch=(ROOT/"tools/patch_upstreams.py").read_text()
        self.assertIn("maizo_syscalls_compat.c",patch)
if __name__=="__main__":
    unittest.main()
