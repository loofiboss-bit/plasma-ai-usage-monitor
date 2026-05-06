#!/usr/bin/env python3
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONITOR_FILES = [
    ROOT / "plugin/claudecodemonitor.cpp",
    ROOT / "plugin/claudecodemonitor.h",
    ROOT / "plugin/codexclimonitor.cpp",
    ROOT / "plugin/codexclimonitor.h",
    ROOT / "plugin/copilotmonitor.cpp",
    ROOT / "plugin/copilotmonitor.h",
    ROOT / "plugin/cursormonitor.cpp",
    ROOT / "plugin/cursormonitor.h",
    ROOT / "plugin/windsurfmonitor.cpp",
    ROOT / "plugin/windsurfmonitor.h",
    ROOT / "plugin/jetbrainsaimonitor.cpp",
    ROOT / "plugin/jetbrainsaimonitor.h",
]

PATTERNS = [
    re.compile(r"return\s+(?:20|30|39|40|50|100|200|225|300|500|900|1000|1125|1500|2000|3000|4500)(?:\.0)?\s*;"),
    re.compile(r"\$[0-9]+"),
    re.compile(r"\b[0-9]+\s*(?:credits/mo|premium requests|messages/5h|/mo)\b", re.IGNORECASE),
]

ALLOW_CONTEXT = (
    "compatibility alias",
    "HTTP",
    "timeout",
    "browser type",
    "pricing lives in subscriptions-v1.json",
)


def main() -> None:
    failures = []
    for path in MONITOR_FILES:
        if not path.exists():
            continue
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            stripped = line.strip()
            if any(marker in stripped for marker in ALLOW_CONTEXT):
                continue
            for pattern in PATTERNS:
                if pattern.search(stripped):
                    failures.append(f"{path.relative_to(ROOT)}:{line_no}: {stripped}")
                    break

    if failures:
        print("Hardcoded pricing check FAIL:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        sys.exit(1)

    print("Hardcoded pricing check OK")


if __name__ == "__main__":
    main()
