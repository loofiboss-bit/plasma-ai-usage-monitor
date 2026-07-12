#!/usr/bin/env python3
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "package/contents/config/main.xml"
CATALOG = ROOT / "package/contents/catalog/providers-v4.json"
NS = {"k": "http://www.kde.org/standards/kcfg/1.0"}

MODEL_KEYS = {
    "openaiModel": "openai",
    "azureModel": "azure",
    "bedrockModel": "bedrock",
    "anthropicModel": "anthropic",
    "googleModel": "google",
    "mistralModel": "mistral",
    "deepseekModel": "deepseek",
    "groqModel": "groq",
    "xaiModel": "xai",
    "ollamaModel": "ollama",
    "openrouterModel": "openrouter",
    "togetherModel": "together",
    "cohereModel": "cohere",
    "googleveoModel": "googleveo",
}


def fail(message: str) -> None:
    print(f"Config default catalog check FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read_config_defaults() -> dict[str, str]:
    try:
        root = ET.parse(CONFIG).getroot()
    except ET.ParseError as exc:
        fail(f"invalid KConfig XML: {exc}")

    defaults: dict[str, str] = {}
    for entry in root.findall(".//k:entry", NS):
        name = entry.get("name", "")
        if name not in MODEL_KEYS:
            continue
        default = entry.findtext("k:default", default="", namespaces=NS).strip()
        if not default:
            fail(f"{name} has an empty default")
        defaults[name] = default
    return defaults


def read_catalog_models() -> dict[str, dict[str, dict]]:
    try:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"invalid provider catalog JSON: {exc}")

    providers: dict[str, dict[str, dict]] = {}
    for provider in catalog.get("providers", []):
        key = provider.get("key")
        if not key:
            continue
        providers[key] = {
            model.get("id"): model
            for model in provider.get("models", [])
            if model.get("id")
        }
    return providers


def main() -> None:
    defaults = read_config_defaults()
    catalog = read_catalog_models()

    missing_keys = sorted(set(MODEL_KEYS) - set(defaults))
    if missing_keys:
        fail(f"missing model defaults in main.xml: {', '.join(missing_keys)}")

    for config_key, provider_key in MODEL_KEYS.items():
        model_id = defaults[config_key]
        provider_models = catalog.get(provider_key)
        if not provider_models:
            fail(f"{config_key} references unknown catalog provider {provider_key}")

        if model_id not in provider_models:
            fail(
                f"{config_key} default '{model_id}' is not in providers-v4.json "
                f"provider '{provider_key}'"
            )

        model = provider_models[model_id]
        if model.get("deprecated") is True:
            fail(f"{config_key} default '{model_id}' is deprecated")

        pricing = model.get("pricing", {})
        if pricing.get("status") == "unknown" and model.get("allowUnknownDefault") is not True:
            fail(f"{config_key} default '{model_id}' has unknown pricing")
        if model.get("allowUnknownDefault") is True and not model.get("unknownDefaultReason"):
            fail(f"{config_key} default '{model_id}' allows unknown pricing without unknownDefaultReason")

        if provider_key == "openrouter" and "/" not in model_id:
            fail(f"{config_key} OpenRouter default '{model_id}' must be a routed model id")

    print(f"Config default catalog check OK: {len(defaults)} provider model defaults validated")


if __name__ == "__main__":
    main()
