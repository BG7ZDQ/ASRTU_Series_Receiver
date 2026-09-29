import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "linux_abi", Path(__file__).parents[1] / "check_linux_abi.py")
abi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(abi)


class LinuxAbiTest(unittest.TestCase):
    def test_only_required_versions(self):
        output = """Version definition section '.gnu.version_d' contains 1 entry:
  0x00: Name: GLIBC_2.39
Version needs section '.gnu.version_r' contains 2 entries:
  0x00: Name: GLIBC_2.2.5  Flags: none
  0x01: Name: GLIBC_2.35  Flags: none
"""
        self.assertEqual(abi.requirements(output), {"GLIBC_2.2.5", "GLIBC_2.35"})
        self.assertEqual(abi.incompatible(abi.requirements(output)), [])

    def test_reject_new_and_named_abi(self):
        names = {"GLIBC_2.36", "GLIBC_2.39", "GLIBC_ABI_DT_RELR", "GLIBC_PRIVATE"}
        self.assertEqual(set(abi.incompatible(names)), names)

    def test_missing_or_empty_input_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(RuntimeError):
                abi.check([root / "missing"])
            with self.assertRaises(RuntimeError):
                abi.check([root])

    def test_appimage_runtime_is_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            runtime = Path(directory) / "receiver.AppImage"
            runtime.write_bytes(b"\x7fELFfixture")
            output = "Version needs section '.gnu.version_r' contains 1 entry:\nName: GLIBC_2.38"
            result = subprocess.CompletedProcess([], 0, output, "")
            with patch.object(abi.subprocess, "run", return_value=result):
                with self.assertRaisesRegex(RuntimeError, "GLIBC_2.38"):
                    abi.check([runtime])

    def test_unreadable_elf_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "broken"
            binary.write_bytes(b"\x7fELF")
            result = subprocess.CompletedProcess([], 1, "", "invalid ELF")
            with patch.object(abi.subprocess, "run", return_value=result):
                with self.assertRaisesRegex(RuntimeError, "Unable to inspect"):
                    abi.check([binary])


if __name__ == "__main__":
    unittest.main()
