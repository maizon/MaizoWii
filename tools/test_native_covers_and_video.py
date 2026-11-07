#!/usr/bin/env python3
"""Contract guardrails for frontend-only VI modes and opt-in cover downloads."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
def read(path):
    return (ROOT/path).read_text(encoding="utf-8")

class NativeCoverAndVideoTests(unittest.TestCase):
    def test_covers_cover_every_game_platform_without_modifying_games(self):
        downloader=read("source/cover_download.c")
        for provider in ("art.gametdb.com", "thumbnails.libretro.com",
                         "Sony - PlayStation","Nintendo - Super Nintendo Entertainment System",
                         "Nintendo - Nintendo Entertainment System",
                         "Nintendo - Game Boy Advance", "Nintendo - Game Boy Color"):
            self.assertIn(provider,downloader)
        self.assertIn("png_image_finish_read",downloader)
        self.assertIn("ART_WIDTH 128",downloader)
        self.assertIn("ART_HEIGHT 176",downloader)
        self.assertIn("CURLOPT_SSL_VERIFYPEER,1L",downloader)
        self.assertIn("CURLOPT_SSL_VERIFYHOST,2L",downloader)
        self.assertNotIn("system(",downloader)
        self.assertIn("cover_forget(&current_game)",downloader)
        self.assertIn("cover_on_disk(&current_game)",downloader)
        self.assertIn("maizo_cover_download_step(&library_state)",read("source/main.c"))
        self.assertNotIn("fetch_png(",read("source/ui.c"))

    def test_video_modes_stay_exclusively_in_maizo_frontend(self):
        sw=read("source/video_sw.c")
        self.assertIn("maizo_video_frontend_select(unsigned selection)",sw)
        self.assertIn("VI_NON_INTERLACE",sw)
        self.assertIn("VI_INTERLACE",sw)
        launch=read("source/launch.c")
        self.assertNotIn("frontend_video_mode",launch)
        self.assertIn("MAIZO-LAUNCH/3",launch)
        self.assertIn("game_x240=",launch)
        self.assertIn("game_x480=",launch)
        main=read("source/main.c")
        self.assertIn("video_preview_frames=600",main)
        self.assertIn("maizo_video_frontend_select(state.video_previous_mode)",main)

    def test_calibration_is_mode_aware_and_older_modules_preserved(self):
        patch=read("tools/patch_upstreams.py")
        self.assertIn("maizo_game_video(EmuSettings.videoMode == VIDEOMODE_ORIGINAL_240P)",patch)
        self.assertIn("WiiSXRX GX gameplay quad",patch)
        self.assertIn("WiiSX 4.2.2 progressive game VI geometry",patch)
        self.assertIn("patch_wiimc(root: Path):",patch)
        self.assertIn("MAIZO_GAME_VIDEO_INC",read("integrations/maizo_game_video.inc"))
        self.assertNotIn("WiiMC",read("integrations/maizo_game_video.inc"))
        self.assertIn("VIDEO_MODULES",read("tools/component_fingerprint.py"))

if __name__=="__main__":
    unittest.main()
