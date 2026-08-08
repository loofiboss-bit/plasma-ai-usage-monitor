#!/usr/bin/env python3
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KCFG = ROOT / "package/contents/config/main.xml"
DIAGNOSTICS_QML = ROOT / "package/contents/ui/configDiagnostics.qml"
RETIRED_LEGACY_KEYS = {"dashboardMode", "showOnlyProblems"}
INTERNAL_RUNTIME_KEYS = {"budgetPolicySelectionRequest"}


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


def qml_key_list(property_name: str) -> set[str]:
    text = DIAGNOSTICS_QML.read_text(encoding="utf-8")
    match = re.search(
        rf"{re.escape(property_name)}:\s*\[(?P<body>.*?)\]",
        text,
        re.S,
    )
    if not match:
        fail(f"could not find {property_name} list in configDiagnostics.qml")
    return set(re.findall(r'"([A-Za-z0-9_]+)"', match.group("body")))


def main() -> None:
    expected = kcfg_keys()
    actual = qml_key_list("portableConfigKeys")
    ignored = qml_key_list("ignoredLegacyConfigKeys")
    missing = expected - INTERNAL_RUNTIME_KEYS - actual
    extra = actual - expected
    if missing:
        fail(f"missing KConfig keys in export schema: {', '.join(sorted(missing))}")
    if extra:
        fail(f"unknown keys in export schema: {', '.join(sorted(extra))}")
    if not INTERNAL_RUNTIME_KEYS <= expected:
        fail("declared internal runtime keys are missing from KConfig")
    internal_overlap = actual & INTERNAL_RUNTIME_KEYS
    if internal_overlap:
        fail(
            "internal runtime keys must not be exported: "
            + ", ".join(sorted(internal_overlap))
        )
    if ignored != RETIRED_LEGACY_KEYS:
        fail(
            "ignored legacy keys must be exactly: "
            + ", ".join(sorted(RETIRED_LEGACY_KEYS))
        )
    overlap = actual & ignored
    if overlap:
        fail(
            "ignored legacy keys remain in the active export schema: "
            + ", ".join(sorted(overlap))
        )
    stale_kconfig = expected & ignored
    if stale_kconfig:
        fail(
            "ignored legacy keys remain in KConfig: "
            + ", ".join(sorted(stale_kconfig))
        )

    qml = DIAGNOSTICS_QML.read_text(encoding="utf-8")
    if "ConfigPortability.schemaV2Settings(" not in qml:
        fail("schema-v2 imports must use the portable settings filter")
    if "ConfigPortability.schemaV3Payload(" not in qml:
        fail("schema-v3 imports must validate settings and policies before mutation")
    if "budgetPolicyRepository.replacePolicies(staged.budgetPolicies)" not in qml:
        fail("schema-v3 policy restore must use the repository transaction boundary")

    print(
        "Config portability check OK: "
        f"{len(actual)} active non-secret keys; "
        f"{len(ignored)} legacy keys accepted and ignored; "
        f"{len(INTERNAL_RUNTIME_KEYS)} local runtime key excluded"
    )


if __name__ == "__main__":
    main()
