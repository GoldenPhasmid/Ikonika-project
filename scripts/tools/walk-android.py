import argparse
import base64
import os
import re
import subprocess
import sys
import time

PACKAGE = "com.goldenphasmid.ikonika"
ACTIVITY = PACKAGE + "/.IkonikaActivity"
LOG = "/sdcard/Android/data/" + PACKAGE + "/files/logs/Ikonika.log"
MOBILE_SCREENS = "today,vocabulary,dictionary,settings,profile,reader"
WALK_TIMEOUT_SECONDS = 60
FAILURE = re.compile(r"NAV FAIL|\| warnings [1-9]")
WARNING = re.compile(r"\] (warning|critical) ")


def adb(*arguments):
    return subprocess.run(["adb", *arguments], capture_output=True, text=True, encoding="utf-8",
                          errors="replace").stdout


def count_log_lines():
    reply = adb("shell", "wc -l < " + LOG).strip()

    return int(reply) if reply.isdigit() else 0


def is_running():
    return adb("shell", "pidof", PACKAGE).strip() != ""


def run_walk(label, variables):
    adb("shell", "am", "force-stop", PACKAGE)

    first_line = count_log_lines() + 1
    encoded = base64.b64encode(variables.encode("utf-8")).decode("ascii")

    adb("shell", "am", "start", "-n", ACTIVITY, "--es", "extraenvvars", encoded)

    time.sleep(2)
    deadline = time.monotonic() + WALK_TIMEOUT_SECONDS

    while is_running() and time.monotonic() < deadline:
        time.sleep(1)

    timed_out = is_running()

    if timed_out:
        print(label + " timed out, stopping the app")
        adb("shell", "am", "force-stop", PACKAGE)

    lines = adb("shell", "tail -n +" + str(first_line) + " " + LOG).splitlines()
    failures = [line.strip() for line in lines if FAILURE.search(line) or WARNING.search(line)]

    if not failures and not timed_out:
        print(label + " passed")

        return 0

    print(label + " FAILED")

    for line in failures:
        print("  " + line)

    return max(len(failures), 1)


def main():
    parser = argparse.ArgumentParser(description="Runs the screen and navigation walks on the emulator.")
    parser.add_argument("--serial", default="emulator-5554", help="adb serial of the device to walk")
    arguments = parser.parse_args()

    os.environ["ANDROID_SERIAL"] = arguments.serial

    if adb("get-state").strip() != "device":
        sys.exit("No device " + arguments.serial + " attached")

    failure_count = run_walk("Screen walk (Android)", "IKONIKA_SCREEN_WALK=" + MOBILE_SCREENS)
    failure_count += run_walk("Navigation walk (Android)", "IKONIKA_NAV_WALK=1")

    sys.exit(1 if failure_count > 0 else 0)


if __name__ == "__main__":
    main()
