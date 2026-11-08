"""Execute the production C argv builder natively, without a Wii or fake mocks."""
import ctypes
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Spec(ctypes.Structure):
    _fields_ = [("system", ctypes.c_int), ("executable", ctypes.c_char_p),
                ("path", ctypes.c_char_p), ("id", ctypes.c_char_p),
                ("wii_ios", ctypes.c_uint), ("wii_usb_port", ctypes.c_uint),
                ("ps1_interpreter", ctypes.c_uint), ("ps1_frameskip", ctypes.c_uint),
                ("ps1_audio", ctypes.c_uint),
                ("ps1_engine", ctypes.c_uint), ("ps1_native", ctypes.c_uint),
                ("ps1_video", ctypes.c_uint), ("ps1_aspect", ctypes.c_uint),
                ("ps1_deflicker", ctypes.c_uint), ("ps1_dither", ctypes.c_uint),
                ("ps1_fps", ctypes.c_uint), ("ps1_limit", ctypes.c_uint),
                ("media_autoboot", ctypes.c_uint)]

class Args(ctypes.Structure):
    _fields_ = [("data", ctypes.c_char * 2048),
                ("argc", ctypes.c_uint), ("length", ctypes.c_size_t)]

class LaunchArgsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("cc")
        if not compiler:
            raise unittest.SkipTest("no host C compiler; dedicated ubuntu contract job must run")
        cls.tmp = tempfile.TemporaryDirectory()
        shared = pathlib.Path(cls.tmp.name) / "libmaizo_launch.so"
        subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-fPIC", "-shared",
                        "-I", str(ROOT / "include"), str(ROOT / "source/launch_args.c"),
                        "-o", str(shared)], check=True, capture_output=True, text=True)
        cls.fn = ctypes.CDLL(str(shared)).maizo_build_args
        cls.fn.argtypes = (ctypes.POINTER(Spec), ctypes.POINTER(Args), ctypes.c_void_p,
                           ctypes.c_size_t)
        cls.fn.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tmp"):
            cls.tmp.cleanup()

    def build(self, system, path, ident=b"RMCE01", **values):
        fields = dict(system=system, executable=b"usb:/apps/module/boot.dol", path=path,
                      id=ident, wii_ios=249, wii_usb_port=0, ps1_interpreter=1,
                      ps1_frameskip=0, ps1_audio=1, ps1_engine=0, ps1_native=0,
                      ps1_video=0, ps1_aspect=0, ps1_deflicker=0,
                      ps1_dither=0, ps1_fps=0, ps1_limit=0, media_autoboot=1)
        fields.update(values)
        spec = Spec(**fields)
        out = Args()
        error = ctypes.create_string_buffer(160)
        ok = self.fn(ctypes.byref(spec), ctypes.byref(out), error, len(error))
        raw = ctypes.string_at(ctypes.addressof(out) + Args.data.offset, out.length)
        if ok:
            self.assertEqual(raw.count(b"\x00"), out.argc)
            self.assertEqual(raw[-1:], b"\x00")
            return raw[:-1].split(b"\x00")
        self.assertTrue(error.value)
        return None

    def test_wii_id6_and_ios(self):
        self.assertEqual(self.build(0, b"usb:/wbfs/Mario Kart/ID.wbfs"),
                         [b"usb:/apps/module/boot.dol", b"RMCE01",
                          b"-ios=249", b"-bootios=249", b"-usbport=0"])
        self.assertIsNone(self.build(0, b"usb:/wbfs/invalid.wbfs", ident=b""))
        self.assertIsNone(self.build(0, b"usb:/wbfs/invalid.wbfs", ident=b"invalid?"))
        self.assertIsNone(self.build(0, b"usb:/wbfs/invalid.wbfs", wii_usb_port=5))

    def test_wii_auto_port_and_loader_ios_are_real_upstream_options(self):
        self.assertEqual(self.build(0, b"usb:/wbfs/Test/RMCE01.wbfs",
                                    wii_ios=250, wii_usb_port=2),
                         [b"usb:/apps/module/boot.dol", b"RMCE01",
                          b"-ios=250", b"-bootios=250", b"-usbport=2"])

    def test_wii_sd_mode_when_game_is_on_sd(self):
        self.assertEqual(self.build(0, b"sd:/wbfs/Mario Kart/RMCE01.wbfs"),
                         [b"usb:/apps/module/boot.dol", b"RMCE01",
                          b"-ios=249", b"-bootios=249", b"-usbport=0", b"-sdmode=1"])

    def test_gamecube_config_file_contract_not_fake_argv(self):
        self.assertEqual(self.build(1, b"sd:/games/Zelda/game.iso"),
                         [b"usb:/apps/module/boot.dol"])
        # Nintendont has 255 bytes including the terminating NUL.
        self.assertEqual(self.build(1, b"sd:/" + b"z" * 253),
                         [b"usb:/apps/module/boot.dol"])
        self.assertIsNone(self.build(1, b"sd:/" + b"z" * 254))

    def test_ps1_argv_and_invalid_format(self):
        self.assertEqual(self.build(2, b"usb:/ps1/Crash Bandicoot.cue"),
                         [b"usb:/apps/module/boot.dol", b"usb:/ps1/",
                          b"Crash Bandicoot.cue", b"Core=1", b"Audio=1", b"SkipFrames=0"])
        self.assertIsNone(self.build(2, b"usb:/ps1/test.chd"))
        self.assertIsNone(self.build(2, b"sd:/ps1/test.M3U"))

    def test_wiisx_422_autoboot_and_real_video_options(self):
        self.assertEqual(self.build(2,b"usb:/ps1/Crash.cue",ps1_engine=1,
                        ps1_native=1,ps1_video=2,ps1_aspect=2,
                        ps1_deflicker=1,ps1_dither=2,ps1_fps=1,ps1_limit=1),
                         [b"usb:/apps/module/boot.dol",b"usb:/ps1/Crash.cue",
                          b"Core=1",b"Audio=1",b"SkipFrames=0",b"NativeOutput=1",
                          b"VideoMode=2",b"ScreenMode=2",b"Deflicker=1",
                          b"Dithering=2",b"FPS=1",b"LimitFrames=1"])
        self.assertIsNone(self.build(2,b"usb:/ps1/Crash.cue",ps1_engine=2))
        self.assertIsNone(self.build(2,b"usb:/ps1/Crash.cue",ps1_engine=1,ps1_video=4))

    def test_gx_emulators_snes_nes_gba_gb(self):
        for system, ext in ((3,b"sfc"),(4,b"nes"),(5,b"gba"),(6,b"gbc")):
            with self.subTest(system=system):
                self.assertEqual(self.build(system, b"sd:/roms/test."+ext),
                                 [b"usb:/apps/module/boot.dol", b"sd:/roms/", b"test."+ext])

    def test_media_conversion_and_printf_protection(self):
        self.assertEqual(self.build(7, b"usb:/videos/movie.mp4"),
                         [b"usb:/apps/module/boot.dol", b"usb1:/videos/movie.mp4"])
        self.assertEqual(self.build(7, b"sd:/music/track.mp3"),
                         [b"usb:/apps/module/boot.dol", b"sd1:/music/track.mp3"])
        self.assertIsNone(self.build(7, b"usb:/videos/movie%p.mp4"))
        self.assertEqual(self.build(7, b"usb:/videos/movie.mp4",media_autoboot=0),
                         [b"usb:/apps/module/boot.dol"])
        self.assertIsNone(self.build(7, b"usb:/videos/movie.mp4",media_autoboot=2))

    def test_homebrew_uses_selected_executable_and_rejects_mismatch(self):
        target=b"sd:/apps/snes9xgx/boot.dol"
        self.assertEqual(self.build(8,target,executable=target),[target])
        self.assertIsNone(self.build(8,target,executable=b"sd:/apps/other/boot.dol"))

    def test_bad_paths_and_system(self):
        self.assertIsNone(self.build(9, b"sd:/games/test.iso"))
        self.assertIsNone(self.build(8, b"sd:/games/test.iso"))
        self.assertIsNone(self.build(3, b"/host/absolute.sfc"))
        self.assertIsNone(self.build(3, b"sd:/roms/"))
        self.assertIsNone(self.build(2, b"sd:/ps1/"))
        self.assertIsNone(self.build(0, b"file", ident=b"ABCD12"))
        self.assertIsNone(self.build(2, b"sd:/ps1/a.bin", ps1_audio=4))

    def test_unsafe_paths_are_rejected(self):
        self.assertIsNone(self.build(2, b"usb:/ps1/../hidden.iso"))
        self.assertIsNone(self.build(3, b"usb:/roms//test.sfc"))
        self.assertIsNone(self.build(3, b"usb:/roms/./test.sfc"))
        self.assertIsNone(self.build(3, b"usb:/"))
        self.assertIsNone(self.build(3, b"usb:/roms/a\\\\b.sfc"))
        self.assertIsNone(self.build(7, b"sd:/videos/test\\rfoo.avi"))

    def test_reject_oversize_before_copy(self):
        self.assertIsNone(self.build(3, b"usb:/roms/" + b"x" * 2500 + b".sfc"))

if __name__ == "__main__":
    unittest.main()
