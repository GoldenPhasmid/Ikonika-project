import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

BUNDLE_ID = "com.goldenphasmid.ikonika"
MOBILE_SCREENS = "today,vocabulary,dictionary,settings,profile,reader"
REGULAR_LOG = "Documents/logs/Ikonika.log"
HEADLESS_ROOT = "tmp/Ikonika-headless"
HEADLESS_LOG = f"{HEADLESS_ROOT}/logs/Ikonika.log"
STARTED = "Startup to first frame"
NARRATION_SENTENCES = "Bonjour.|Le chat dort sur la table."
SHARED_BOOK_NAME = "Essai partage.fb2"
SHARED_BOOK = """<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
<description><title-info><book-title>Essai partage</book-title><lang>fr</lang></title-info></description>
<body><section><p>Bonjour. Le chat dort sur la table. Il fait beau aujourd'hui.</p></section></body>
</FictionBook>
"""
FAILURE = re.compile(r"NAV FAIL|\| warnings [1-9]|\] critical |Package install failed")
WARNING = re.compile(r"\] warning ")


def simctl(*arguments, check=True):
    result = subprocess.run(["xcrun", "simctl", *arguments], capture_output=True, text=True)

    if check and result.returncode != 0:
        sys.exit(f"simctl {' '.join(arguments)} failed: {result.stderr.strip()}")

    return result.stdout.strip()


def pick_iphone():
    devices = json.loads(simctl("list", "devices", "available", "--json"))["devices"]

    for runtime in sorted((name for name in devices if "iOS" in name), reverse=True):
        for device in devices[runtime]:
            if device["name"].startswith("iPhone"):
                return device["udid"]

    sys.exit("No available iPhone simulator")


def read_lines(path):
    if not path.exists():
        return []

    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def file_name(label):
    return label.lower().replace(" ", "-")


class Simulator:
    def __init__(self, device, output):
        self.device = device
        self.output = output
        self.failures = 0

    def container(self):
        return Path(simctl("get_app_container", self.device, BUNDLE_ID, "data"))

    def is_running(self):
        return f"UIKitApplication:{BUNDLE_ID}" in simctl("spawn", self.device, "launchctl", "list", check=False)

    def launch(self, arguments, variables, log):
        simctl("terminate", self.device, BUNDLE_ID, check=False)
        time.sleep(2)

        if log.exists():
            log.unlink()

        environment = dict(os.environ)
        environment.update({f"SIMCTL_CHILD_{key}": value for key, value in variables.items()})

        result = subprocess.run(["xcrun", "simctl", "launch", self.device, BUNDLE_ID, *arguments],
                                capture_output=True, text=True, env=environment)

        if result.returncode != 0:
            sys.exit(f"Launch failed: {result.stderr.strip()}")

    def find_marker(self, log, markers):
        for line in read_lines(log):
            if any(marker in line for marker in markers):
                return line

        return None

    def wait_for(self, log, markers, timeout):
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            found = self.find_marker(log, markers)

            if found is not None:
                return found

            if not self.is_running():
                return self.find_marker(log, markers)

            time.sleep(5)

        return None

    def phase(self, label, arguments, variables, log_name, markers, timeout, strict, action=None, required=()):
        print(f"== {label}", flush=True)

        log = self.container() / log_name
        self.launch(arguments, variables, log)

        if action is not None:
            action(log)

        found = self.wait_for(log, markers, timeout)

        simctl("io", self.device, "screenshot", str(self.output / f"{file_name(label)}.png"), check=False)

        lines = read_lines(log)
        (self.output / f"{file_name(label)}.log").write_text("\n".join(lines), encoding="utf-8")

        pattern = re.compile(f"{FAILURE.pattern}|{WARNING.pattern}") if strict else FAILURE
        problems = [line.strip() for line in lines if pattern.search(line)]

        if found is None:
            problems.append(f"no line with {markers} within {timeout} seconds, app running: {self.is_running()}")

        for line in required:
            if not any(line in logged for logged in lines):
                problems.append(f"no line with '{line}'")

        if problems:
            print(f"{label} FAILED")

            for line in problems:
                print(f"  {line}")

            self.failures += 1

            return

        print(f"{label} passed: {found.strip()}")

    def share_book(self, log):
        if self.wait_for(log, [STARTED], 300) is None:
            return

        book = self.container() / "tmp" / SHARED_BOOK_NAME
        book.write_text(SHARED_BOOK, encoding="utf-8")
        time.sleep(3)

        print(f"  handing over {book.as_uri()}", flush=True)
        result = subprocess.run(["xcrun", "simctl", "openurl", self.device, book.as_uri()], capture_output=True, text=True)

        if result.returncode != 0:
            print(f"  simctl openurl failed: {result.stderr.strip()}", flush=True)

    def report(self, label, needle):
        for line in read_lines(self.output / f"{file_name(label)}.log"):
            if needle in line:
                print(f"  {line.strip()}", flush=True)

    def check_dictionaries(self, label):
        directory = self.container() / HEADLESS_ROOT / "dictionaries"

        for database in sorted(directory.rglob("*.db")):
            result = subprocess.run(["sqlite3", "-readonly", str(database), "PRAGMA quick_check;"],
                                    capture_output=True, text=True)
            verdict = (result.stdout + result.stderr).strip().splitlines()[:3]
            print(f"  {label}: {database.name} {database.stat().st_size} bytes, quick_check {verdict}", flush=True)


def boot(arguments):
    arguments.output.mkdir(parents=True, exist_ok=True)

    device = pick_iphone()
    print(f"Simulator {device}", flush=True)
    (arguments.output / "simulator-udid.txt").write_text(device, encoding="utf-8")

    simctl("boot", device, check=False)


def walk(arguments):
    device = (arguments.output / "simulator-udid.txt").read_text(encoding="utf-8").strip()

    simctl("bootstatus", device, "-b")
    print("Simulator booted", flush=True)
    simctl("install", device, str(arguments.app))

    simulator = Simulator(device, arguments.output)

    simulator.phase("First launch", [], {}, REGULAR_LOG, [STARTED], 300, strict=False)

    simulator.phase("Dictionary install", ["-headless"], {"IKONIKA_INSTALL_PACKAGE": arguments.dictionary},
                    HEADLESS_LOG, ["Package install finished", "Package install failed"], 900, strict=False)
    time.sleep(10)
    simulator.check_dictionaries("after install")

    simulator.phase("Screen walk", ["-headless", "-keep-dictionaries"], {"IKONIKA_SCREEN_WALK": MOBILE_SCREENS},
                    HEADLESS_LOG, ["screen walk done"], 900, strict=True)
    simulator.check_dictionaries("after screen walk")
    simulator.report("Screen walk", "Translation model for")

    simulator.phase("Navigation walk", ["-headless", "-keep-dictionaries"], {"IKONIKA_NAV_WALK": "1"},
                    HEADLESS_LOG, ["navigation walk done"], 900, strict=True)

    simulator.phase("Platform checks", ["-headless", "-keep-dictionaries"],
                    {"IKONIKA_NARRATION_CHECK": NARRATION_SENTENCES, "IKONIKA_CHECKPOINT_CHECK": "1"},
                    HEADLESS_LOG, ["narration check done", "narration check failed"], 120, strict=True,
                    required=["checkpoint check done"])

    simulator.phase("Shared book", ["-headless", "-keep-dictionaries"], {}, HEADLESS_LOG,
                    ["Reader: opening a book handed over by another app"], 120, strict=True,
                    action=simulator.share_book)

    simctl("terminate", device, BUNDLE_ID, check=False)

    sys.exit(1 if simulator.failures else 0)


def main():
    parser = argparse.ArgumentParser(description="Boots an iPhone simulator, then installs the app in it, checks "
                                                 "the first launch, installs a dictionary and runs the screen walks.")
    commands = parser.add_subparsers(dest="command", required=True)

    boot_command = commands.add_parser("boot", help="Pick an iPhone simulator and start booting it")
    boot_command.add_argument("output", type=Path, help="Directory for screenshots and logs")

    walk_command = commands.add_parser("walk", help="Install the app in the booted simulator and walk it")
    walk_command.add_argument("app", type=Path, help="Built IkonikaApp.app for the simulator")
    walk_command.add_argument("output", type=Path, help="Directory for screenshots and logs")
    walk_command.add_argument("--dictionary", default="dictionary-fr", help="Catalog package the walks learn with")

    arguments = parser.parse_args()

    if arguments.command == "boot":
        boot(arguments)
    else:
        walk(arguments)


if __name__ == "__main__":
    main()
