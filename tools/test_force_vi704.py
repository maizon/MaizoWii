"""Force VI 704 option invariants; not a substitute for physical CRT QA."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def code(name): return (ROOT/name).read_text(encoding="utf8")

class Force704Tests(unittest.TestCase):
    def test_default_is_off_and_persistent(self):
        s=code("source/config.c")
        self.assertIn("c->force_vi704=0;",s)
        self.assertIn('else if(!strcmp(key,"force_vi704"))c->force_vi704=!!val;',s)
        self.assertIn("force_vi704=%u",s)
    def test_general_settings_are_live(self):
        s=code("source/settings.c")
        main=code("source/main.c")
        self.assertIn("case SYSTEM_COUNT:return 6",s)
        self.assertIn('FORCAR VI 704',s)
        self.assertIn("c->force_vi704=!c->force_vi704",s)
        self.assertIn("maizo_video_force_vi704(config.force_vi704)",main)
    def test_frontend_writes_vi_not_fake_fb_resolution(self):
        s=code("source/video_sw.c")
        self.assertIn("force_vi704?704u:original_vi_width*cal_w/100u",s)
        self.assertIn("mode->viWidth=(u16)width",s)
        self.assertIn("VIDEO_Configure(mode)",s)
        self.assertNotIn("mode->fbWidth=704",s)
    def test_gc_nintendont_704_is_exact(self):
        s=code("source/launch.c")
        self.assertIn("int scale=c->force_vi704?104:",s)
        self.assertIn("n.VideoScale=(s8)scale",s)
        self.assertIn("force_vi704=%u",s)
    def test_240p_and_other_engine_paths(self):
        game=code("integrations/maizo_game_video.inc")
        patch=code("tools/patch_upstreams.py")
        self.assertIn("int width=maizo_game_force_vi704()?704:",game)
        self.assertIn("patch_gx_vi704(root)",patch)
        self.assertIn('maizo_video_adjust_vi(rmode,1)',patch)
        self.assertIn('nes_gba_video',patch)
        self.assertIn("MaizoForceVI704",code("integrations/maizo_wiimc_settings.inc"))
        self.assertIn('maizo_patch_game_vi704(dst,len)',patch)
        self.assertIn("GXRModeObjRVL",code("integrations/maizo_wii_vi704.inc"))
        self.assertIn("maizo_usbloader_menu_vi704.inc",patch)
        self.assertIn("maizo_loader_menu_vi704_requested()",patch)
    def test_untouched_unadapted_modules(self):
        doc=code("docs/CRT_VI704.md")
        self.assertIn("não",doc.lower())
        self.assertIn("homebrews",doc.lower())

if __name__=="__main__": unittest.main()
