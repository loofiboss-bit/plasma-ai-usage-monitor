#!/usr/bin/env python3
"""Keep declarative QML registrations aligned with the native module sources."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin"
CMAKE = PLUGIN / "CMakeLists.txt"
REGISTRATION = re.compile(
    r"\bQML_(?:ELEMENT|NAMED_ELEMENT|ANONYMOUS|UNCREATABLE|SINGLETON)\b"
)


def cmake_list(text: str, name: str) -> set[str]:
    match = re.search(
        rf"set\s*\(\s*{re.escape(name)}(.*?)\)", text, re.DOTALL | re.IGNORECASE
    )
    if match is None:
        raise SystemExit(f"Error: Could not find {name} in plugin/CMakeLists.txt")
    return set(match.group(1).split())


def main() -> None:
    cmake = CMAKE.read_text(encoding="utf-8")
    headers = cmake_list(cmake, "aiusagemonitor_HDRS")
    registered_headers = {
        path.name
        for path in PLUGIN.glob("*.h")
        if REGISTRATION.search(path.read_text(encoding="utf-8"))
    }

    if not registered_headers:
        raise SystemExit("Error: No declaratively registered QML types found")

    missing = sorted(registered_headers - headers)
    if missing:
        raise SystemExit(
            "Error: Declarative QML headers missing from aiusagemonitor_HDRS: "
            + ", ".join(missing)
        )

    forbidden = (
        "NO_GENERATE_PLUGIN_SOURCE",
        "CLASS_NAME AiUsagePlugin",
        "aiusageplugin.cpp",
        "aiusageplugin.h",
    )
    stale = [token for token in forbidden if token in cmake]
    if stale:
        raise SystemExit(
            "Error: Native QML module still uses manual plugin registration: "
            + ", ".join(stale)
        )

    print(
        "QML registered types consistency OK: "
        f"{len(registered_headers)} declarative headers"
    )


if __name__ == "__main__":
    main()
