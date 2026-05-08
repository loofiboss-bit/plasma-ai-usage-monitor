#!/usr/bin/env python3
import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "package/contents/catalog/providers-v3.json"
EXPECTED_KEYS = {
    "openai", "anthropic", "google", "mistral", "deepseek", "groq", "xai",
    "ollama", "openrouter", "together", "cohere", "googleveo", "azure",
    "bedrock",
}
TOKEN_UNITS = {"1M_tokens"}
NON_TOKEN_UNITS = {"generation", "image", "video_second", "credit", "request", "unknown", "not_applicable"}


def fail(message: str) -> None:
    print(f"Provider catalog check FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def require_iso_date(value: str, label: str) -> date:
    try:
        return date.fromisoformat(value)
    except ValueError:
        fail(f"{label} must be ISO date YYYY-MM-DD")


def require_source_refs(refs, context: str) -> None:
    if not isinstance(refs, list) or not refs:
        fail(f"{context} missing sourceRefs")
    for ref in refs:
        if not isinstance(ref, dict):
            fail(f"{context} sourceRef must be an object")
        if not ref.get("label") or not ref.get("url"):
            fail(f"{context} sourceRef missing label/url")
        require_iso_date(str(ref.get("reviewedAt", "")), f"{context} sourceRef reviewedAt")


def main() -> None:
    if not CATALOG.exists():
        fail(f"missing {CATALOG}")

    try:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON: {exc}")

    if catalog.get("schemaVersion") != 3:
        fail("schemaVersion must be 3")
    if catalog.get("runtimeScraping") is not False:
        fail("runtimeScraping must be false")

    reviewed_date = require_iso_date(str(catalog.get("lastReviewed", "")), "lastReviewed")
    if (date.today() - reviewed_date).days > 45:
        fail(f"catalog lastReviewed is stale: {reviewed_date}")

    providers = catalog.get("providers")
    if not isinstance(providers, list):
        fail("providers must be a list")

    seen = set()
    manual_review = 0
    source_conflict = 0

    for provider in providers:
        if not isinstance(provider, dict):
            fail("provider must be an object")
        key = provider.get("key")
        if not key:
            fail("provider missing key")
        if key in seen:
            fail(f"duplicate provider key: {key}")
        seen.add(key)

        for field in ("label", "dataQuality", "pricingFreshness"):
            if not provider.get(field):
                fail(f"{key} missing {field}")
        if provider.get("needsManualReview") is True:
            manual_review += 1
            if not provider.get("reviewReason"):
                fail(f"{key} needsManualReview entries must include reviewReason")
        if provider.get("sourceConflict") is True:
            source_conflict += 1
            if not provider.get("sourceConflictReason"):
                fail(f"{key} sourceConflict entries must include sourceConflictReason")

        models = provider.get("models")
        if not isinstance(models, list) or not models:
            fail(f"{key} models must be a non-empty list")

        model_ids = set()
        for model in models:
            if not isinstance(model, dict):
                fail(f"{key} model must be an object")
            model_id = model.get("id")
            if not model_id:
                fail(f"{key} model missing id")
            if model_id in model_ids:
                fail(f"{key} duplicate model id: {model_id}")
            model_ids.add(model_id)
            require_source_refs(model.get("sourceRefs"), f"{key}/{model_id}")

            pricing = model.get("pricing")
            if not isinstance(pricing, dict):
                fail(f"{key}/{model_id} missing pricing")
            if "precision" not in pricing:
                fail(f"{key}/{model_id} pricing.precision missing")

            status = pricing.get("status")
            unit = pricing.get("unit")
            if unit in TOKEN_UNITS:
                if "input" not in pricing or "output" not in pricing:
                    fail(f"{key}/{model_id} token pricing must include input and output")
                for field in ("input", "output", "cachedInput"):
                    if field in pricing and pricing[field] < 0:
                        fail(f"{key}/{model_id} {field} must be non-negative")
            elif unit in NON_TOKEN_UNITS or status in {"unknown", "not_applicable"}:
                if pricing.get("input") == 0.0 and pricing.get("output") == 0.0:
                    fail(f"{key}/{model_id} non-token pricing must not fake 0.0 token pricing")
            else:
                fail(f"{key}/{model_id} unsupported pricing.unit/status")

    missing = EXPECTED_KEYS - seen
    extra = seen - EXPECTED_KEYS
    if missing:
        fail(f"missing provider keys: {', '.join(sorted(missing))}")
    if extra:
        fail(f"unexpected provider keys: {', '.join(sorted(extra))}")

    suffix = ""
    if manual_review or source_conflict:
        suffix = f"; manualReview={manual_review}, sourceConflict={source_conflict}"
    print(f"Provider catalog check OK: {len(providers)} providers, reviewed {catalog['lastReviewed']}{suffix}")


if __name__ == "__main__":
    main()
