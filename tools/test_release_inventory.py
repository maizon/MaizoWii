#!/usr/bin/env python3
"""Never equate green CI with hardware-complete 1.0."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
class InventoryTests(unittest.TestCase):
    def test_exact_63_requirements(self):
        doc=(ROOT/"docs/RELEASE_1_0_63.md").read_text()
        rows=[]
        for line in doc.splitlines():
            cols=[part.strip() for part in line.split("|")]
            if len(cols)==5 and cols[1].isdigit():
                rows.append((cols[1],cols[2],cols[3]))
        self.assertEqual(len(rows),63)
        self.assertEqual([int(x[0]) for x in rows],list(range(1,64)))
        self.assertEqual(len({x[1] for x in rows}),63)
        self.assertGreater(sum(x[2]!="implementado" for x in rows),0)
        self.assertIn("NÃO É UMA RELEASE",doc)
    def test_one_package_not_duplicate_qa_zips(self):
        build=(ROOT/".github/workflows/build.yml").read_text()
        self.assertIn("name: MAIZOWII-ALL-IN-ONE", build)
        self.assertNotIn("MAIZOWII-QA-SAFE-NO-WAD",build)
        self.assertNotIn("MAIZOWII-PREVIEW-USB-SD",build)
        self.assertNotIn("-id MZIO",build)
        self.assertIn("channel/standard/Maizo-Frontend-MIZN-SD-USB.wad",build)
        self.assertIn("if: false",build)
if __name__=="__main__":
    unittest.main()
