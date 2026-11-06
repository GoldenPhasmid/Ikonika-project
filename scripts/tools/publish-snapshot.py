import argparse
import io
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROJECT_VERSION = re.compile(r"project\(Ikonika VERSION ([0-9.]+)")

EXCLUDED = [
    ".claude",
    "scripts/dictionary",
    "scripts/opus",
    "third_party/eflomal",
]


def run(arguments, cwd):
    result = subprocess.run(arguments, cwd=cwd, check=False, capture_output=True)

    if result.returncode != 0:
        output = (result.stdout + result.stderr).decode(errors="replace").strip()
        sys.exit(f"{' '.join(arguments)} failed with exit code {result.returncode}.\n{output}")

    return result.stdout.decode(errors="replace")


def read_version(revision):
    match = PROJECT_VERSION.search(run(["git", "show", f"{revision}:CMakeLists.txt"], ROOT))

    if match is None:
        sys.exit("The top-level CMakeLists.txt has no project(Ikonika VERSION ...) call.")

    return match.group(1)


def require_clean_tree():
    if run(["git", "status", "--porcelain"], ROOT).strip():
        sys.exit("The private repository has uncommitted changes. Commit them first, so the snapshot "
                 "matches a commit.")


def is_excluded(name):
    return any(name == path or name.startswith(path + "/") for path in EXCLUDED)


def export_tree(revision, destination):
    archive = subprocess.run(["git", "archive", "--format=tar", revision], cwd=ROOT, check=True,
                             capture_output=True).stdout

    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        members = [member for member in tar.getmembers() if not is_excluded(member.name)]
        tar.extractall(destination, members=members, filter="data")


def scan_for_secrets(tree):
    if shutil.which("gitleaks") is None:
        sys.exit("gitleaks is not on PATH. Install it with \"winget install Gitleaks.Gitleaks\", or pass "
                 "--skip-scan to copy without a scan.")

    print("Scanning the snapshot with gitleaks")
    run(["gitleaks", "dir", str(tree), "--config", str(ROOT / ".gitleaks.toml"), "--no-banner", "--redact",
         "--verbose"], tree)


def replace_contents(public, tree):
    for entry in public.iterdir():
        if entry.name == ".git":
            continue

        if entry.is_dir():
            shutil.rmtree(entry)
        else:
            entry.unlink()

    shutil.copytree(tree, public, dirs_exist_ok=True)


def main():
    parser = argparse.ArgumentParser(
        description="Replace the working tree of a local clone of the public source repository with a "
                    "commit of this private repository, ready to commit there by hand. Nothing is "
                    "committed or pushed.")
    parser.add_argument("public", type=Path, help="Local clone of the public source repository")
    parser.add_argument("--revision", default="HEAD", help="Private commit to copy, HEAD by default")
    parser.add_argument("--skip-scan", action="store_true", help="Copy without the gitleaks scan")
    arguments = parser.parse_args()

    public = arguments.public.resolve()

    if not (public / ".git").exists():
        sys.exit(f"{public} is not a git clone.")

    if public == ROOT:
        sys.exit("The public clone cannot be this repository.")

    if (public / "catalog.json").exists():
        sys.exit(f"{public} holds catalog.json, so it is the releases repository, not the source one.")

    if arguments.revision == "HEAD":
        require_clean_tree()

    version = read_version(arguments.revision)
    revision = run(["git", "rev-parse", "--short", arguments.revision], ROOT).strip()

    with tempfile.TemporaryDirectory(prefix="ikonika-snapshot-") as temporary:
        tree = Path(temporary)
        export_tree(arguments.revision, tree)

        if not arguments.skip_scan:
            scan_for_secrets(tree)

        replace_contents(public, tree)

    changed = run(["git", "status", "--porcelain", "--untracked-files=all"], public).splitlines()

    print(f"Copied {revision}, version {version}, into {public}")
    print(f"Excluded: {', '.join(EXCLUDED)}")
    print(f"{len(changed)} paths changed. Review and commit them there:")
    print("    git add --all")
    print(f"    git commit -m \"Ikonika {version}\"")


if __name__ == "__main__":
    main()
