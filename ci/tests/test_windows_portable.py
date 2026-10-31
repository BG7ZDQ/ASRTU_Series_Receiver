import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("portable", REPO / "ci/package_windows_portable.py")
portable = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(portable)


class WindowsPortableTest(unittest.TestCase):
    def test_complete_layout_and_no_build_manifests(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "stage"
            for name in (*portable.REQUIRED, "bin/README.txt", "sdrpp/sdrpp.exe"):
                path = stage / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")
            (stage / "core-files.iss").write_text("build-only manifest")
            archive = portable.package(stage, root / "out", REPO)
            with zipfile.ZipFile(archive) as bundle:
                prefix = archive.stem + "/"
                names = {name.removeprefix(prefix) for name in bundle.namelist()}
                self.assertTrue(set(portable.REQUIRED).issubset(names))
                self.assertIn("sdrpp/sdrpp.exe", names)
                self.assertIn("LICENSE", names)
                self.assertIn("SDRSHARP_NOTICE.txt", names)
                self.assertIn("THIRD_PARTY_NOTICE.txt", names)
                self.assertNotIn("core-files.iss", names)
                self.assertNotIn("bin/README.txt", names)
                self.assertIn(b"bin\\ASRTU1_Launcher.exe", bundle.read(prefix + "README.txt"))

    def test_rejects_incomplete_runtime(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(ValueError, "Missing portable runtime"):
                portable.package(root, root / "out", REPO)
            self.assertFalse((root / "out").exists())


if __name__ == "__main__":
    unittest.main()
