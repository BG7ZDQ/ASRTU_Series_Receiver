from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
FILES = [
    ROOT / "apps" / "launcher" / "suite_launcher.cpp",
    ROOT / "apps" / "doppler" / "satellite_tracker_dialog.cpp",
    ROOT / "apps" / "dsp" / "main_window.cpp",
]
CALL = re.compile(r'QStringLiteral\("((?:\\.|[^"\\])*)"\)')
CHINESE = re.compile(r"[\u3400-\u9fff]")


for path in FILES:
    source = path.read_text(encoding="utf-8")

    def replace(match: re.Match[str]) -> str:
        text = match.group(1)
        if not CHINESE.search(text):
            return match.group(0)
        return f'QCoreApplication::translate("ASRTU", "{text}")'

    path.write_text(CALL.sub(replace, source), encoding="utf-8")
