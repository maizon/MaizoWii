#!/usr/bin/env python3
"""Host-side C contract tests for cover-only HTTP downgrade safety.
Network/TLS on a physical Wii is NOT exercised by this test.
"""
from pathlib import Path
import ctypes,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]

class CoverTransportTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cc=shutil.which("gcc") or shutil.which("cc")
  if not cc: raise unittest.SkipTest("host C compiler unavailable")
  cls.temp=tempfile.TemporaryDirectory()
  cls.libpath=Path(cls.temp.name)/"libtransport.so"
  subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
      "-shared","-fPIC","-I",str(ROOT/"include"),
      str(ROOT/"source/cover_transport.c"),"-o",str(cls.libpath)],
      check=True,capture_output=True,text=True)
  cls.lib=ctypes.CDLL(str(cls.libpath))
  cls.lib.maizo_cover_gametdb_http.argtypes=[
      ctypes.c_char_p,ctypes.c_char_p,ctypes.c_size_t]
  cls.lib.maizo_cover_gametdb_http.restype=ctypes.c_int
  cls.lib.maizo_cover_allow_legacy_retry.argtypes=[
      ctypes.c_uint,ctypes.c_int,ctypes.c_char_p]
  cls.lib.maizo_cover_allow_legacy_retry.restype=ctypes.c_int
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def convert(self,url,cap=512):
  out=ctypes.create_string_buffer(cap)
  yes=self.lib.maizo_cover_gametdb_http(url.encode(),out,cap)
  return yes,out.value.decode()
 def test_exact_gametdb_cover_only(self):
  for kind in ("cover","cover3D","coverfull","coverfullHQ","disc"):
   src=f"https://art.gametdb.com/wii/{kind}/US/RMCE01.png"
   self.assertEqual(self.convert(src),(1,src.replace("https://","http://",1)))
  self.assertEqual(self.convert("https://art.gametdb.com/gc/cover3D/JA/GZLE01.png"),
                   (1,"http://art.gametdb.com/gc/cover3D/JA/GZLE01.png"))
 def test_rejects_untrusted_hosts_and_payloads(self):
  invalid=[
   "https://evil.com/wii/cover/US/RMCE01.png",
   "https://art.gametdb.com.evil.com/wii/cover/US/RMCE01.png",
   "https://art.gametdb.com/wii/cover/US/RMCE01.dol",
   "https://art.gametdb.com/wii/cover/US/../../payload.png",
   "https://art.gametdb.com/wii/cover/US/RMCE01.png?token=1",
   "https://art.gametdb.com/wiimc/cover/US/RMCE01.png",
   "https://art.gametdb.com/wii/cover/US/RMCE0.png",
   "https://art.gametdb.com/wii/cover/US/RMCE01.png/extra",
   "http://art.gametdb.com/wii/cover/US/RMCE01.png",
   "https://art.gametdb.com/wii/cover/US",
   "https://art.gametdb.com/",
  ]
  for src in invalid:
   with self.subTest(url=src):self.assertEqual(self.convert(src)[0],0)
 def test_http_only_on_certificate_failure_and_allowed_modes(self):
  src=b"https://art.gametdb.com/wii/cover/US/RMCE01.png"
  self.assertTrue(self.lib.maizo_cover_allow_legacy_retry(0,60,src))
  self.assertTrue(self.lib.maizo_cover_allow_legacy_retry(2,60,src))
  self.assertFalse(self.lib.maizo_cover_allow_legacy_retry(1,60,src))
  for code in (0,1,7,22,35,77):
   self.assertFalse(self.lib.maizo_cover_allow_legacy_retry(0,code,src))
 def test_output_buffer_too_small_is_safe(self):
  src="https://art.gametdb.com/wii/cover/US/RMCE01.png"
  self.assertEqual(self.convert(src,12)[0],0)

class DownloadPipelineContracts(unittest.TestCase):
 def get(self,p):return (ROOT/p).read_text(encoding="utf-8")
 def test_https_verification_remains_on(self):
  s=self.get("source/cover_download.c")
  self.assertIn("CURLOPT_SSL_VERIFYPEER,1L",s)
  self.assertIn("CURLOPT_SSL_VERIFYHOST,2L",s)
  self.assertNotIn("CURLOPT_SSL_VERIFYPEER,0L",s)
  self.assertIn("maizo_cover_gametdb_http",s)
  self.assertIn("CURLE_PEER_FAILED_VERIFICATION",s)
  self.assertIn("CURLOPT_ERRORBUFFER,curl_error",s)
  self.assertIn("CURLOPT_CAINFO_BLOB,&ca_bundle_blob",s)
  self.assertIn("https://raw.githubusercontent.com/libretro-thumbnails/",s)
 def test_one_persisted_user_network_mode(self):
  self.assertIn('cover_transport=%u',self.get("source/config.c"))
  self.assertIn('cover_transport=val<=2?val:0',self.get("source/config.c"))
  self.assertIn('maizo_cover_download_set_transport(config.cover_transport)',
                self.get("source/main.c"))
  ui=self.get("source/ui.c")
  self.assertIn("USB LOADER GX HTTP",ui)
  self.assertIn("CERTIFICADO SSL",ui)
 def test_updated_bundle_on_frontend_build(self):
  wf=self.get(".github/workflows/build.yml")
  self.assertIn("https://curl.se/ca/cacert.pem",wf)
  self.assertIn("maizo-frontend-v3-trust-",wf)

if __name__=="__main__":unittest.main()
