#!/usr/bin/env python3
"""Host tests for the Rev.5 firmware: build and run them with g++ and Node.js, no ESP32 needed.

    python3 tests/run_host_tests.py

- tco_test: the JJY time code (TcoValue()) against a separately written encoder, and the pulse timing of TcoGen()
  with a simulated Ticker, including SNTP clock steps. The code under test is cut from BF-018ARev5.ino.
- tz_test: the POSIX TZ check of the device (WebUiValidTimeZone() in BF_WebUi.cpp) and of the page (validTz() in
  BF_WebUiPage.h) against tests/host/tz_cases.txt, so the two can't drift apart.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
REV5 = ROOT / "BF-018ARev5"
HOST = ROOT / "tests" / "host"


def cut(text, start, end, what):
    """The text from the line holding `start` up to (not including) the line holding `end`."""
    a = text.find(start)
    b = text.find(end, a + 1)
    if a < 0 or b < 0:
        sys.exit(f"run_host_tests.py: can't find {what}; update the markers in this script")
    a = text.rfind("\n", 0, a) + 1
    b = text.rfind("\n", 0, b) + 1
    return text[a:b]


def run(cmd, **kwargs):
    print("$", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kwargs)


def main():
    sketch = (REV5 / "BF-018ARev5.ino").read_text(encoding="utf-8")
    webui = (REV5 / "BF_WebUi.cpp").read_text(encoding="utf-8")
    page = (REV5 / "BF_WebUiPage.h").read_text(encoding="utf-8")

    with tempfile.TemporaryDirectory() as tmp:
        build = pathlib.Path(tmp)
        # TcoGen() up to the web UI section, which includes TcOn(), TcOff(), CarrierWrite(), TcoValue() and the BCD helpers
        (build / "tco.inc").write_text(cut(sketch, "// main task of TCO", "// web UI: the status the page shows", "TcoGen()"))
        (build / "tz.inc").write_text(cut(webui, "// POSIX TZ strings", "// settings\n", "WebUiValidTimeZone()"))
        js = cut(page, "const TZ_CHARS", "// The LEDC timer divides", "validTz()")
        (build / "tz_test.js").write_text(
            js + "\nconst lines = require('fs').readFileSync(0, 'utf8').split('\\n')"
                 ".filter(l => l && l[0] !== '#');\n"
                 "lines.forEach(l => console.log((validTz(l.slice(2)) ? 1 : 0) + ' ' + l.slice(2)));\n")

        flags = ["-std=c++17", "-O2", "-Wall", "-Werror", "-Wno-unused-function", "-I", str(build)]
        run(["g++", *flags, "-o", build / "tco_test", HOST / "tco_test.cpp"])
        run(["g++", *flags, "-o", build / "tz_test", HOST / "tz_test.cpp"])

        run([build / "tco_test"])
        cases = (HOST / "tz_cases.txt").read_text()
        device = run([build / "tz_test"], input=cases, capture_output=True, text=True).stdout
        browser = run(["node", build / "tz_test.js"], input=cases, capture_output=True, text=True).stdout
        if device != browser:
            print("FAIL: the device's and the page's TZ checks disagree:")
            for d, b in zip(device.splitlines(), browser.splitlines()):
                if d != b:
                    print(f"  device {d!r}, page {b!r}")
            sys.exit(1)
        print(f"tz_test: passed, {len(device.splitlines())} cases, device and page agree")


if __name__ == "__main__":
    main()
