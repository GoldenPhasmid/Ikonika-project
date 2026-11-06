import json
import sys

devices = json.load(sys.stdin)["devices"]
runtimes = sorted((runtime for runtime in devices if "iOS" in runtime), reverse=True)

for runtime in runtimes:
    for device in devices[runtime]:
        if device["name"].startswith("iPhone"):
            print(device["udid"])
            sys.exit(0)

sys.exit("No available iPhone simulator")
