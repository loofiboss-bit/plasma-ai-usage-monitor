#!/usr/bin/env python3
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KCFG = ROOT / "package/contents/config/main.xml"
DIAGNOSTICS_QML = ROOT / "package/contents/ui/configDiagnostics.qml"


def fail(message: str) -> None:
    print(f"Config portability check FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def kcfg_keys() -> set[str]:
    ns = {"k": "http://www.kde.org/standards/kcfg/1.0"}
    try:
        root = ET.parse(KCFG).getroot()
    except ET.ParseError as exc:
        fail(f"invalid KConfig XML: {exc}")
    return {
        entry.attrib["name"]
        for entry in root.findall(".//k:entry", ns)
        if "name" in entry.attrib
    }


def portable_keys() -> set[str]:
    text = DIAGNOSTICS_QML.read_text(encoding="utf-8")
    match = re.search(r"portableConfigKeys:\s*\[(?P<body>.*?)\]\s*Dialogs\.FileDialog", text, re.S)
    if not match:
        fail("could not find portableConfigKeys list in configDiagnostics.qml")
    return set(re.findall(r'"([A-Za-z0-9_]+)"', match.group("body")))


def main() -> None:
    expected = kcfg_keys()
    actual = portable_keys()
    missing = expected - actual
    extra = actual - expected
    if missing:
        fail(f"missing KConfig keys in export schema: {', '.join(sorted(missing))}")
    if extra:
        fail(f"unknown keys in export schema: {', '.join(sorted(extra))}")
    print(f"Config portability check OK: {len(actual)} non-secret KConfig keys covered")


if __name__ == "__main__":
    main()
