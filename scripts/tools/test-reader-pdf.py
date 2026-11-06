import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qt-bin", type=Path, default=Path("C:/Qt/6.11.1/msvc2022_64/bin"))
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="ikonika-reader-test-") as temporary:
        destination = Path(temporary)
        shutil.copytree(root / "app/tests/qml/reader", destination, dirs_exist_ok=True)
        for source in (root / "app/qml/mobile").glob("MobileReaderPdf*.qml"):
            shutil.copy2(source, destination)
        for name in ("DesktopReaderTextView.qml", "DesktopScrollBar.qml"):
            shutil.copy2(root / "app/qml/desktop" / name, destination)
        for name in ("MobileReaderFrame.qml", "MobileReaderPageNumber.qml", "MobileTextField.qml"):
            source = root / "app/qml/mobile" / name
            if source.exists():
                shutil.copy2(source, destination)
        environment = os.environ.copy()
        environment["PATH"] = str(arguments.qt_bin) + os.pathsep + environment["PATH"]
        environment["QT_QPA_PLATFORM"] = "offscreen"
        environment["QT_QUICK_BACKEND"] = "software"
        environment["QT_QPA_FONTDIR"] = str(Path(environment.get("WINDIR", "C:/Windows")) / "Fonts")
        report = destination / "results.txt"
        result = subprocess.run(
            [str(arguments.qt_bin / "qmltestrunner.exe"), "-input", str(destination),
             "-o", str(report) + ",txt"],
            env=environment,
            capture_output=True,
            text=True,
        )
        print(result.stdout, end="")
        print(result.stderr, end="")
        if report.exists():
            print(report.read_text(encoding="utf-8"), end="")
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
