"""Exercise installer safety and generated launchers with a fake AppImage."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
BASH = (shutil.which("bash") if os.name != "nt" else
        "C:/Program Files/Git/bin/bash.exe")


def posix(path):
    value = Path(path).resolve().as_posix()
    if os.name == "nt":
        value = "/" + value[0].lower() + value[2:]
    return value


@unittest.skipUnless(BASH and Path(BASH).exists(), "Bash is required")
class LinuxInstallerTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="asrtu-installer-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.user = self.root / "user"
        self.user.mkdir()
        self.bundle = self.root / "bundle"
        self.bundle.mkdir()
        self.script = self.bundle / "setup.sh"
        shutil.copyfile(REPO / "packaging/linux/setup.sh", self.script)
        payload = self.bundle / "ASRTU_Series_Receiver-test-LinuxAppImage"
        payload.mkdir()
        app = payload / "test.AppImage"
        app.write_text("""#!/usr/bin/env bash
set -eu
mkdir -p squashfs-root
cat >squashfs-root/AppRun <<'APP'
#!/usr/bin/env bash
printf 'launched:%s\\n' "$1"
APP
chmod +x squashfs-root/AppRun
""", encoding="utf-8")
        self.env = dict(os.environ, HOME=posix(self.user),
                        TMPDIR=posix(self.root))

    def run_setup(self, prefix, *args, success=True, language="en"):
        result = subprocess.run(
            [BASH, posix(self.script), "--lang", language,
             "--prefix", posix(prefix), "--without-sdrpp", "--yes", *args],
            env=self.env, capture_output=True, text=True, encoding="utf-8",
            timeout=30)
        self.assertEqual(result.returncode == 0, success,
                         result.stdout + result.stderr)
        return result

    def test_reject_unmanaged_and_home_for_install_and_uninstall(self):
        unrelated = self.user / "personal"
        unrelated.mkdir()
        sentinel = unrelated / "keep.txt"
        sentinel.write_text("keep")
        for target in (unrelated, self.user, self.root):
            for args in ((), ("--uninstall",)):
                self.run_setup(target, *args, success=False)
                self.assertEqual(sentinel.read_text(), "keep")

    def test_refuse_invalid_marker_and_missing_uninstall(self):
        prefix = self.user / "receiver"
        prefix.mkdir()
        (prefix / ".asrtu-installation").write_text("not ASRTU")
        self.run_setup(prefix, success=False)
        self.run_setup(prefix, "--uninstall", success=False)
        self.run_setup(self.user / "absent", "--uninstall", success=False)

    def test_install_upgrade_uninstall_and_space_shortcut(self):
        prefix = self.user / "ASRTU receiver"
        self.run_setup(prefix)
        self.assertTrue((prefix / ".asrtu-installation").is_file())
        self.run_setup(prefix)
        desktop = self.user / ".local/share/applications/asrtu-series-receiver.desktop"
        entry = desktop.read_text(encoding="utf-8")
        command = next(x[5:] for x in entry.splitlines() if x.startswith("Exec="))
        self.assertEqual(shlex.split(command), [posix(prefix / "AppRun")])
        for executable in (self.user / ".local/bin/asrtu", prefix / "AppRun"):
            result = subprocess.run([BASH, posix(executable), "hello world"],
                                    env=self.env, capture_output=True, text=True,
                                    timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), "launched:hello world")
        recording = self.user / "recording.wav"
        recording.write_text("preserve")
        self.run_setup(prefix, "--uninstall")
        self.assertFalse(prefix.exists())
        self.assertFalse(desktop.exists())
        self.assertEqual(recording.read_text(), "preserve")

    @unittest.skipIf(os.name == "nt", "POSIX filename and symlink semantics")
    def test_metacharacters_and_symlink(self):
        prefix = self.user / 'receiver $cash `tick` "quote" \\slash %field'
        self.run_setup(prefix)
        entry = (self.user / ".local/share/applications/asrtu-series-receiver.desktop").read_text()
        command = next(x[5:] for x in entry.splitlines() if x.startswith("Exec="))
        # Decode Desktop Entry string escapes, then Exec quotes/field codes.
        decoded = re.sub(r"\\(.)", lambda m: {"s": " ", "n": "\n", "t": "\t",
                         "r": "\r", "\\": "\\"}[m[1]], command)
        decoded = decoded.replace("%%", "%")
        result = subprocess.run([BASH, "-c", decoded + " 'hello world'"],
                                env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "launched:hello world")
        link = self.user / "alias"
        link.symlink_to(prefix, target_is_directory=True)
        result = subprocess.run([BASH, posix(self.script), "--lang", "en",
                                 "--uninstall", "--prefix", str(link) + "/"],
                                env=self.env, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((prefix / "AppRun").exists())

    def test_safety_messages_are_localized(self):
        for language, expected in (("zh", "拒绝"), ("ja", "専用"),
                                   ("en", "Refusing")):
            result = self.run_setup(self.user, success=False, language=language)
            self.assertIn(expected, result.stderr)


if __name__ == "__main__":
    unittest.main()
