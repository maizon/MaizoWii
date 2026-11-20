#!/usr/bin/env python3
"""Regression contracts: Wii CRT mode select and cover downloader timeouts.
These check source logic; they do NOT simulate analog CRT or Wii WLAN.
"""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding="utf-8")

class FrontendOnlyHotfix(unittest.TestCase):
 def test_tv_mode_confirm_does_not_advance_to_next_standard(self):
  main=read("source/main.c")
  settings=read("source/settings.c")
  header=read("include/app.h")
  self.assertIn("int settings_is_tv_standard(System s,int row)",settings)
  self.assertIn("return row==maizo_standard_row(s);",settings)
  self.assertIn("settings_is_tv_standard(System,int)",header)
  self.assertIn("if(i->right)option_change(1);",main)
  self.assertIn("if(i->accept && !settings_is_tv_standard(scope,state.settings_row))",main)
  self.assertIn("if(i->accept && settings_is_tv_standard(scope,state.settings_row))",main)
  self.assertNotIn("if(i->right||i->accept)option_change(1);",main)
  self.assertIn("state.tv_preview_pending=0;state.video_preview_frames=0;\n    return;",main)
  for standard in ("AUTO","NTSC","PAL50","PAL60","PAL-M"):
   self.assertIn(standard,settings)
 def test_https_errors_fail_over_without_downgrade_by_default(self):
  net=read("source/cover_download.c")
  transport=read("source/cover_transport.c")
  self.assertIn("CURLOPT_CONNECTTIMEOUT,15L",net)
  self.assertIn("CURLOPT_TIMEOUT,32L",net)
  self.assertIn("CURLOPT_IPRESOLVE,CURL_IPRESOLVE_V4",net)
  self.assertIn("CURLE_PEER_FAILED_VERIFICATION",net)
  self.assertIn("CURLE_OPERATION_TIMEDOUT",net)
  self.assertIn("unreachable_hosts&host",net)
  self.assertIn("CERTIFICADO EXPIRADO",net)
  self.assertIn("transport_setting==2",net)
  self.assertNotIn("gametdb_tls_broken",net)
  self.assertIn("return mode==2&&curl_error==60",transport)
  self.assertIn("CURLOPT_SSL_VERIFYPEER,1L",net)
  self.assertIn("CURLOPT_SSL_VERIFYHOST,2L",net)
  self.assertNotIn("CURLOPT_SSL_VERIFYPEER,0L",net)
 def test_ci_build_only_frontend_on_marker(self):
  wf=read(".github/workflows/build.yml")
  self.assertIn("[frontend-only]",wf)
  self.assertIn("FRONTEND_ONLY",wf)
  self.assertIn("if [ \"$FRONTEND_ONLY\" != \"true\" ]; then",wf)
  self.assertIn("maizo-frontend-v3-trust-",wf)
  self.assertIn("MAIZOWII-FRONTEND",wf)

if __name__=="__main__":unittest.main()
