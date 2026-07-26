#!/usr/bin/env python3
"""Reject QML translation calls that bypass Plasma's localized context."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "package" / "contents" / "ui"
FORBIDDEN = ("KI18n.i18n", "KI18n.i18nc", "KI18n.i18np")


def main() -> None:
    violations: list[str] = []

    for path in sorted(UI.rglob("*.qml")):
        text = path.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), start=1):
            for token in FORBIDDEN:
                if token in line:
                    violations.append(
                        f"{path.relative_to(ROOT)}:{line_number}: {token}"
                    )

    if violations:
        raise SystemExit(
            "Error: QML must use Plasma's global i18n functions; "
            "qualified KI18n calls fail at runtime:\n" + "\n".join(violations)
        )

    print("QML localization contract OK: global Plasma i18n functions only")


if __name__ == "__main__":
    main()
