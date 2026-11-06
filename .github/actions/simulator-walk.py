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
HEADLESS_LOG = "tmp/Ikonika-headless/logs/Ikonika.log"
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

    def wait_for(self, log, markers, timeout):
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            for line in read_lines(log):
                if any(marker in line for marker in markers):
                    return line

            if not self.is_running():
                return None

            time.sleep(5)

        return None

    def phase(self, label, arguments, variables, log_name, markers, timeout, strict):
        print(f"== {label}", flush=True)

        log = self.container() / log_name
        self.launch(arguments, variables, log)

        found = None

        if markers:
            found = self.wait_for(log, markers, timeout)
        else:
            time.sleep(timeout)

        simctl("io", self.device, "screenshot", str(self.output / f"{file_name(label)}.png"), check=False)

        lines = read_lines(log)
        (self.output / f"{file_name(label)}.log").write_text("\n".join(lines), encoding="utf-8")

        pattern = re.compile(f"{FAILURE.pattern}|{WARNING.pattern}") if strict else FAILURE
        problems = [line.strip() for line in lines if pattern.search(line)]

        if markers and found is None:
            problems.append(f"no line with {markers} within {timeout} seconds, app running: {self.is_running()}")

        if not markers and not self.is_running():
            problems.append(f"the app is not running {timeout} seconds after launch")

        if problems:
            print(f"{label} FAILED")

            for line in problems:
                print(f"  {line}")

            self.failures += 1

            return

        print(f"{label} passed{': ' + found.strip() if found else ''}")


def main():
    parser = argparse.ArgumentParser(description="Installs the app in an iPhone simulator, checks the first "
                                                 "launch, installs a dictionary and runs the screen walks.")
    parser.add_argument("app", type=Path, help="Built IkonikaApp.app for the simulator")
    parser.add_argument("output", type=Path, help="Directory for screenshots and logs")
    parser.add_argument("--dictionary", default="dictionary-fr", help="Catalog package the walks learn with")
    arguments = parser.parse_args()

    arguments.output.mkdir(parents=True, exist_ok=True)

    device = pick_iphone()
    print(f"Simulator {device}", flush=True)
    (arguments.output / "simulator-udid.txt").write_text(device, encoding="utf-8")

    simctl("boot", device, check=False)
    simctl("bootstatus", device, "-b")
    simctl("install", device, str(arguments.app))

    simulator = Simulator(device, arguments.output)

    simulator.phase("First launch", [], {}, REGULAR_LOG, [], 120, strict=False)

    simulator.phase("Dictionary install", ["-headless"], {"IKONIKA_INSTALL_PACKAGE": arguments.dictionary},
                    HEADLESS_LOG, ["Package install finished", "Package install failed"], 900, strict=False)

    simulator.phase("Screen walk", ["-headless", "-keep-dictionaries"], {"IKONIKA_SCREEN_WALK": MOBILE_SCREENS},
                    HEADLESS_LOG, ["screen walk done"], 900, strict=True)

    simulator.phase("Navigation walk", ["-headless", "-keep-dictionaries"], {"IKONIKA_NAV_WALK": "1"},
                    HEADLESS_LOG, ["navigation walk done"], 900, strict=True)

    simctl("terminate", device, BUNDLE_ID, check=False)

    sys.exit(1 if simulator.failures else 0)


if __name__ == "__main__":
    main()
