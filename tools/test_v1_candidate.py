#!/usr/bin/env python3
"""v1 candidate regression contracts. These are host-side checks, NOT hardware proof."""
from pathlib import Path
import re
import unittest
R = Path(__file__).resolve().parents[1]
def code(path): return (R / path).read_text(encoding="utf-8")

class CandidateContracts(unittest.TestCase):
    def test_start_is_scoped_and_b_returns_home(self):
        main=code("source/main.c"); ui=code("source/ui.c")
        self.assertIn("state.settings_global=state.page==PAGE_HOME",main)
        self.assertIn("state.settings_return=state.page",main)
        self.assertIn("settings_count(scope)",main)
        self.assertIn("state.page=PAGE_HOME;state.in_system=0",main)
        self.assertIn("if(u->in_system){if(topbar_phase<8)topbar_phase++;topbar(u,l,hid);}",ui)
        self.assertIn("CONFIGURACOES DO SISTEMA",ui)
        self.assertIn("CONFIGURACOES GERAIS",ui)
    def test_real_alpha_icons_preserve_home_portraits(self):
        prep=code("tools/prepare_system_icons.py")
        video=code("source/video_sw.c")
        cache=code("source/covers.c")
        self.assertIn("transparent_pack",prep)
        self.assertIn("for transparent in (False,True)",prep)
        self.assertIn('asset=("T" if transparent else "I")',prep)
        self.assertIn("void maizo_video_icon(",video)
        self.assertIn("int alpha=src[offset]",video)
        self.assertIn("cover_draw_icon(",cache)
        self.assertIn("static Cache cache[27]",cache)
    def test_art_sources_are_real_and_cancelable(self):
        dl=code("source/cover_download.c")
        for marker in ("https://art.gametdb.com", "https://thumbnails.libretro.com",
                       "curl_multi_perform", "curl_multi_remove_handle",
                       "CURLOPT_SSL_VERIFYHOST,2L", "provider_url(",
                       "received.length>=8", "save_png_as_gxt",
                       "failed_indices", "chosen_art", "chosen_region"):
            self.assertIn(marker,dl)
        self.assertNotIn("fetch_png(",dl)
    def test_manager_exposes_options_and_coverage(self):
        main=code("source/main.c"); ui=code("source/ui.c")
        for token in ("maizo_cover_download_custom","covers_mode","covers_single",
                      "config.cover_source","config.cover_art","config.cover_region"):
            self.assertIn(token,main)
        for token in ("REPETIR FALHAS","SELECIONAR TODOS",
                      "JOGO SELECIONADO","RECONSTRUIR CACHE RAM"):
            self.assertIn(token,ui)
        for token in ("cover_source=%u","cover_art=%u","cover_region=%u"):
            self.assertIn(token,code("source/config.c"))
    def test_offline_gametdb_has_real_id_title_records(self):
        p=R/"data/wiitdb-en.txt"
        rows=p.read_text(encoding="utf-8").splitlines()
        matching=[line for line in rows if re.match(r"^[A-Z0-9]{6} = .+",line)]
        self.assertGreater(len(matching),5000)
        lib=code("source/library.c")
        self.assertIn("bsearch(&key,titles",lib)
        self.assertIn("metadata_begin();",lib)
        self.assertIn("official_title(g->id)",lib)
        pack=code("tools/package_all.py")
        self.assertIn('out / "maizo/database/wiitdb-en.txt"',pack)
    def test_nointro_importer_generates_real_aliases(self):
        import importlib.util
        import tempfile
        from xml.etree import ElementTree as ET
        spec=importlib.util.spec_from_file_location("metaimport",R/"tools/import_nointro_dat.py")
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/"test.dat"
            path.write_text('<datafile><game name="Official Name (USA)"><rom name="ROM filename (USA).sfc"/></game></datafile>',encoding="utf-8")
            records=module.import_dat("SNES",path)
            self.assertEqual(records[("SNES","ROM filename (USA)")][0],"Official Name (USA)")
    def test_release_acceptance_matrix_complete(self):
        doc=code("docs/RELEASE_V1_0_ACCEPTANCE.md")
        for i in range(1,81):
            self.assertIn(f"| R{i:03d} |",doc)
        self.assertIn("NÃO VALIDADO NO WII REAL",doc)

if __name__=="__main__": unittest.main()
