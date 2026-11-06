#!/usr/bin/env python3
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ICONS_DIR = ROOT / "app" / "qml" / "mobile" / "icons"
OUTPUT_FILE = ROOT / "app" / "qml" / "mobile" / "MobileIcons.js"

VIEWBOX_RE = re.compile(r'viewBox="([^"]+)"')
PATH_D_RE = re.compile(r'<path\s+d="([^"]+)"')


def parse_icon(svg_path):
    text = svg_path.read_text(encoding="utf-8")

    viewbox_match = VIEWBOX_RE.search(text)
    if not viewbox_match:
        raise ValueError(f"{svg_path}: missing viewBox")

    min_x, min_y, width, height = (float(value) for value in viewbox_match.group(1).split())

    path_ds = PATH_D_RE.findall(text)
    if not path_ds:
        raise ValueError(f"{svg_path}: no <path d=...> found")

    return {
        "path": " ".join(path_ds),
        "minX": min_x,
        "minY": min_y,
        "width": width,
        "height": height,
    }


def format_number(value):
    if value == int(value):
        return str(int(value))
    return repr(value)


def format_icon(name, icon):
    return (
        f'    {name}: {{ path: "{icon["path"]}", '
        f'minX: {format_number(icon["minX"])}, minY: {format_number(icon["minY"])}, '
        f'width: {format_number(icon["width"])}, height: {format_number(icon["height"])} }}'
    )


def main():
    svg_files = sorted(ICONS_DIR.glob("*.svg"))

    if not svg_files:
        print(f"No .svg files found in {ICONS_DIR}", file=sys.stderr)
        return 1

    entries = []

    for svg_file in svg_files:
        name = svg_file.stem.replace("-", "_")
        icon = parse_icon(svg_file)
        entries.append(format_icon(name, icon))

    OUTPUT_FILE.write_text(
        ".pragma library\n\nvar Icons = {\n" + ",\n".join(entries) + "\n}\n",
        encoding="utf-8",
    )

    print(f"Wrote {len(svg_files)} icons to {OUTPUT_FILE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
