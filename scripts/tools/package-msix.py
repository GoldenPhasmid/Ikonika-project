import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CMAKE = Path(r"C:\Qt\Tools\CMake_64\bin\cmake.exe")
WINDOWS_KITS_BIN = Path(r"C:\Program Files (x86)\Windows Kits\10\bin")
MANIFEST_TEMPLATE = ROOT / "packaging" / "msix" / "AppxManifest.xml.in"
APP_ICON = ROOT / "app" / "icons" / "AppIcon.png"

LOGO_SIZES = {
    "StoreLogo.png": 50,
    "Square44x44Logo.png": 44,
    "Square150x150Logo.png": 150,
}


def read_project_version():
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(\s*Ikonika\s+VERSION\s+(\d+)\.(\d+)\.(\d+)", text)

    if match is None:
        sys.exit("No project(Ikonika VERSION x.y.z) call in the top-level CMakeLists.txt")

    return tuple(int(part) for part in match.groups())


def to_store_version(project_version):
    major, minor, patch = project_version

    return f"{major + 1}.{minor}.{patch}.0"


def find_makeappx():
    candidates = [
        path for path in WINDOWS_KITS_BIN.glob("10.*/x64/makeappx.exe")
    ]

    if not candidates:
        sys.exit(f"makeappx.exe not found under {WINDOWS_KITS_BIN}. Install the Windows SDK.")

    return max(candidates, key=lambda path: tuple(int(part) for part in path.parts[-3].split(".")))


def run(command):
    print(" ".join(str(part) for part in command), flush=True)

    result = subprocess.run(command)

    if result.returncode != 0:
        sys.exit(result.returncode)


def write_logos(stage):
    assets = stage / "Assets"
    assets.mkdir(parents=True, exist_ok=True)

    with Image.open(APP_ICON) as icon:
        for name, size in LOGO_SIZES.items():
            icon.resize((size, size), Image.LANCZOS).save(assets / name)


def write_manifest(stage, store_version):
    template = MANIFEST_TEMPLATE.read_text(encoding="utf-8")
    manifest = stage / "AppxManifest.xml"

    manifest.write_text(template.format(Version=store_version), encoding="utf-8")

    return manifest


def main():
    parser = argparse.ArgumentParser(description="Packs the installed app tree into an MSIX for the Microsoft Store.")
    parser.add_argument("--config", default="Release", choices=["Release", "Debug"])
    parser.add_argument("--register", action="store_true",
                        help="Register the staged tree with Windows for a local run. Needs developer mode.")
    arguments = parser.parse_args()

    build = ROOT / "build" / f"msvc-{arguments.config.lower()}"

    if not (build / "CMakeCache.txt").exists():
        sys.exit(f"{build} is not configured. Run build-msvc.ps1 first.")

    store_version = to_store_version(read_project_version())
    stage = build / "msix" / "stage"

    if stage.exists():
        shutil.rmtree(stage)

    run([CMAKE, "--install", build, "--config", arguments.config, "--prefix", stage])

    write_logos(stage)
    manifest = write_manifest(stage, store_version)

    if arguments.register:
        run(["powershell", "-NoProfile", "-Command", f"Add-AppxPackage -Register '{manifest}'"])
        print(f"Registered Ikonika {store_version} from {stage}")

        return

    package = build / f"Ikonika-{store_version}.msix"

    run([find_makeappx(), "pack", "/d", stage, "/p", package, "/o"])

    print(f"Packed {package}")


if __name__ == "__main__":
    main()
