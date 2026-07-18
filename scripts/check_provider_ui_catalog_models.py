#!/usr/bin/env python3
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "package/contents/catalog/providers-v4.json"
PROVIDER_UI = ROOT / "package/contents/ui/configProviders.qml"
PROVIDER_DETAILS = ROOT / "package/contents/ui/ProviderSourceDetails.qml"
PROVIDER_REGISTRY = ROOT / "package/contents/ui/ProviderRegistry.qml"
COST_SUMMARY = ROOT / "package/contents/ui/CostSummaryCard.qml"

NATIVE_BACKENDS = {
    "openai",
    "anthropic",
    "google",
    "mistral",
    "deepseek",
    "groq",
    "xai",
    "ollama",
    "openrouter",
    "together",
    "cohere",
    "googleveo",
    "azure",
    "bedrock",
}


def fail(message: str) -> None:
    print(f"Provider UI catalog check FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    provider_keys = [entry.get("key", "") for entry in catalog.get("providers", [])]
    if not provider_keys or any(not key for key in provider_keys):
        fail("provider catalog has missing or empty provider keys")

    source = PROVIDER_UI.read_text(encoding="utf-8") + PROVIDER_DETAILS.read_text(encoding="utf-8")
    references = Counter(re.findall(r'catalogModelIds\("([a-z0-9_-]+)"\)', source))
    generic_picker = "catalogModelIds(details.descriptor.configKey)" in source
    if not generic_picker:
        missing = sorted(set(provider_keys) - set(references))
        extra = sorted(set(references) - set(provider_keys))
        duplicates = sorted(key for key, count in references.items() if count != 1)
        if missing:
            fail(f"model picker is not catalog-driven for: {', '.join(missing)}")
        if extra:
            fail(f"model picker references unknown providers: {', '.join(extra)}")
        if duplicates:
            fail(f"providers must have exactly one catalog model picker: {', '.join(duplicates)}")

    # A provider picker must not silently grow a second hardcoded model list.
    catalog_ids = {
        model.get("id")
        for provider in catalog.get("providers", [])
        for model in provider.get("models", [])
        if model.get("id")
    }
    embedded_ids = sorted(model_id for model_id in catalog_ids if f'"{model_id}"' in source)
    if embedded_ids:
        fail(f"hardcoded catalog model IDs remain in provider UI: {', '.join(embedded_ids)}")

    registry = PROVIDER_REGISTRY.read_text(encoding="utf-8")
    missing_backends = sorted(
        key
        for key in NATIVE_BACKENDS
        if f'case "{key}": return registry.{key}Backend;' not in registry
    )
    if missing_backends:
        fail(f"native provider backends are not stable in the registry: {', '.join(missing_backends)}")

    cost_summary = COST_SUMMARY.read_text(encoding="utf-8")
    mode_delegate = re.search(
        r"PlasmaComponents\.ToolButton\s*\{(?P<body>.*?)\n\s*\}", cost_summary, re.S
    )
    if not mode_delegate or "required property var modelData" not in mode_delegate.group("body"):
        fail("cost view mode delegate must declare modelData under bound component behavior")

    print(f"Provider UI catalog check OK: {len(provider_keys)} model pickers are catalog-driven")


if __name__ == "__main__":
    main()
