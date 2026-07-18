#!/usr/bin/env python3
"""Validate catalog-driven KCM bindings and transactional secret hooks."""

from __future__ import annotations

import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "package/contents/catalog/providers-v4.json"
CONFIG = ROOT / "package/contents/config/main.xml"
UI = ROOT / "package/contents/ui"


def fail(message: str) -> None:
    print(f"KCM contract check FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def declared_cfg_properties(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    return set(
        re.findall(
            r"\bproperty\s+(?:alias|bool|int|real|double|string|var)\s+cfg_([A-Za-z0-9_]+)",
            text,
        )
    )


def require_bindings(
    providers: list[dict], config_field: str, qml_file: str, *, capability: str | None = None
) -> None:
    path = UI / qml_file
    declared = declared_cfg_properties(path)
    missing: list[str] = []

    for provider in providers:
        if capability and capability not in provider.get("capabilities", []):
            continue
        config_key = provider.get("config", {}).get(config_field, "")
        if config_key and config_key not in declared:
            missing.append(f"{provider.get('key', '<unknown>')}:{config_key}")

    if missing:
        fail(f"{qml_file} lacks {config_field} bindings: {', '.join(missing)}")


def validate_secret_pages() -> None:
    for name in ("configProviders.qml", "configSubscriptions.qml", "configAlerts.qml"):
        text = (UI / name).read_text(encoding="utf-8")
        if "Component.onDestruction" in text:
            fail(f"{name} must not persist secrets from Component.onDestruction")
        if not re.search(r"\bunsavedChanges\s*:\s*secretChanges\.dirty\b", text):
            fail(f"{name} does not expose SecretChangeSet dirtiness")
        if not re.search(r"\bfunction\s+saveConfig\s*\(", text):
            fail(f"{name} does not commit from Plasma's saveConfig hook")


def validate_diagnostics() -> None:
    text = (UI / "configDiagnostics.qml").read_text(encoding="utf-8")
    forbidden = ("konsole --hold", "sh -c", "install_doctor.sh", "show_installed_versions.sh")
    found = [token for token in forbidden if token in text]
    if found:
        fail(f"Diagnostics still contains shell-launch tokens: {', '.join(found)}")
    if "plasmashell --version; rpm -q plasma-ai-usage-monitor" not in text:
        fail("Diagnostics does not expose the copyable version-check command")
    if not re.search(r'troubleshootingUrl:\s*"https://', text):
        fail("Diagnostics troubleshooting action must use HTTPS")


def main() -> None:
    try:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
        config_root = ET.parse(CONFIG).getroot()
    except (json.JSONDecodeError, ET.ParseError, OSError) as exc:
        fail(str(exc))

    providers = catalog.get("providers", [])
    if not isinstance(providers, list):
        fail("provider catalog does not contain a providers list")

    config_keys = {
        element.attrib["name"]
        for element in config_root.iter()
        if element.tag.endswith("entry") and "name" in element.attrib
    }
    for provider in providers:
        for field, key in provider.get("config", {}).items():
            if field != "key" and key not in config_keys:
                fail(f"{provider.get('key')} config.{field} references missing KConfig key {key}")

    require_bindings(providers, "refreshInterval", "configGeneral.qml")
    require_bindings(providers, "notifications", "configAlerts.qml")
    require_bindings(providers, "enabled", "configProviders.qml")
    require_bindings(providers, "model", "configProviders.qml")
    require_bindings(providers, "dailyBudget", "configBudget.qml", capability="cost")
    require_bindings(providers, "monthlyBudget", "configBudget.qml", capability="cost")
    validate_secret_pages()
    validate_diagnostics()

    print(
        f"KCM contract check OK: {len(providers)} providers, transactional secrets, safe diagnostics"
    )


if __name__ == "__main__":
    main()
