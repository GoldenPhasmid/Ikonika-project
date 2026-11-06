import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[2]
    qt_bin = Path("C:/Qt/6.11.1/msvc2022_64/bin")
    with tempfile.TemporaryDirectory(prefix="ikonika-progress-test-") as temporary:
        destination = Path(temporary)
        shutil.copytree(root / "app/tests/qml/progress", destination, dirs_exist_ok=True)
        for name in ("DesktopSessionProgress.qml", "DesktopProgressBar.qml", "DesktopDivider.qml"):
            shutil.copy2(root / "app/qml/desktop" / name, destination)
        environment = os.environ.copy()
        environment["PATH"] = str(qt_bin) + os.pathsep + environment["PATH"]
        environment["QT_QPA_PLATFORM"] = "offscreen"
        environment["QT_QUICK_BACKEND"] = "software"
        report = destination / "results.txt"
        result = subprocess.run([str(qt_bin / "qmltestrunner.exe"), "-input", str(destination),
                                 "-o", str(report) + ",txt"], env=environment, capture_output=True, text=True)
        print(result.stdout, end="")
        print(result.stderr, end="")
        if report.exists():
            print(report.read_text(encoding="utf-8"), end="")
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
