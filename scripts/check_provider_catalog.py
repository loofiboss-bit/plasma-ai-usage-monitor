#!/usr/bin/env python3
import json
import sys
import xml.etree.ElementTree as ET
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "package/contents/catalog/providers-v4.json"
EXPECTED_KEYS = {
    "openai", "anthropic", "google", "mistral", "deepseek", "groq", "xai",
    "ollama", "openrouter", "together", "cohere", "googleveo", "azure",
    "bedrock", "litellm", "cerebras", "fireworks", "perplexity",
}
TOKEN_UNITS = {"1M_tokens"}
NON_TOKEN_UNITS = {"generation", "image", "video_second", "credit", "request", "unknown", "not_applicable"}
ADAPTER_TYPES = {
    "account_usage",
    "account_balance",
    "anthropic_usage_cost",
    "model_discovery",
    "gateway_usage",
}
MONITORING_LEVELS = {"actual_usage_spend", "actual_key_usage", "balance_connectivity", "gateway_aggregate", "connectivity_only"}
METRIC_SOURCES = {
    "billing_api", "usage_api", "metrics_api", "response_headers",
    "published_documentation", "local_observation", "estimated_pricing",
    "connectivity_probe", "model_discovery_api",
}
PROVIDER_BUDGET_SCOPES = {
    "openai": ["aggregate", "project", "line_item"],
    "anthropic": ["aggregate", "workspace", "model", "service_tier", "line_item"],
    "openrouter": ["aggregate"],
    "litellm": ["aggregate"],
}
SUPPORTED_BILLING_CYCLES = {
    "calendar_day", "iso_week", "calendar_month", "anchored_month", "provider_reset"
}


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

    if catalog.get("schemaVersion") != 6:
        fail("schemaVersion must be 6")
    if catalog.get("runtimeScraping") is not False:
        fail("runtimeScraping must be false")

    reviewed_date = require_iso_date(str(catalog.get("lastReviewed", "")), "lastReviewed")
    if (date.today() - reviewed_date).days > 45:
        fail(f"catalog lastReviewed is stale: {reviewed_date}")

    providers = catalog.get("providers")
    if not isinstance(providers, list):
        fail("providers must be a list")

    seen = set()
    config_keys = {
        entry.attrib.get("name")
        for entry in ET.parse(ROOT / "package/contents/config/main.xml").iter()
        if entry.tag.endswith("entry")
    }
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
        for field in ("stableId", "displayName", "dbName", "icon", "colorToken", "adapterType", "monitoringLevel",
                      "auth", "endpoint", "capabilities", "expectedSources",
                      "probePolicy", "reviewExpiresAt", "config"):
            if not provider.get(field):
                fail(f"{key} missing v4 field {field}")
        if provider["stableId"] != key:
            fail(f"{key} stableId must match key")
        if provider["adapterType"] not in ADAPTER_TYPES:
            fail(f"{key} adapterType is invalid")
        if provider["monitoringLevel"] not in MONITORING_LEVELS:
            fail(f"{key} monitoringLevel is invalid")
        if not isinstance(provider["capabilities"], list) or not provider["capabilities"]:
            fail(f"{key} capabilities must be a non-empty list")
        if not isinstance(provider["expectedSources"], list) or not set(provider["expectedSources"]) <= METRIC_SOURCES:
            fail(f"{key} expectedSources contains an unknown Metric Contract source")
        if key in PROVIDER_BUDGET_SCOPES and provider.get("supportedBudgetScopes") != PROVIDER_BUDGET_SCOPES[key]:
            fail(f"{key} supportedBudgetScopes does not match the validated provider dimensions")
        if key in PROVIDER_BUDGET_SCOPES:
            if provider.get("budgetPolicyContractVersion") != "budget-policy-v2":
                fail(f"{key} must declare budget-policy-v2")
            cycles = provider.get("supportedBillingCycles")
            if not isinstance(cycles, list) or not cycles or not set(cycles) <= SUPPORTED_BILLING_CYCLES:
                fail(f"{key} supportedBillingCycles is missing or invalid")
            reviewed = require_iso_date(str(provider.get("capabilityReviewedAt", "")), f"{key} capabilityReviewedAt")
            capability_expiry = require_iso_date(
                str(provider.get("capabilityReviewExpiresAt", "")),
                f"{key} capabilityReviewExpiresAt",
            )
            if capability_expiry < date.today() or capability_expiry < reviewed:
                fail(f"{key} budget capability review is expired or precedes its review")
        elif any(field in provider for field in (
            "budgetPolicyContractVersion", "supportedBudgetScopes", "supportedBillingCycles",
            "capabilityReviewedAt", "capabilityReviewExpiresAt",
        )):
            fail(f"{key} declares an unreviewed budget policy capability")
        auth = provider["auth"]
        if not isinstance(auth, dict) or not auth.get("scheme") or "credentialSlots" not in auth:
            fail(f"{key} auth must declare scheme and credentialSlots")
        if auth.get("scheme") != "none" and not auth.get("credentialSlots"):
            fail(f"{key} authenticated profiles need credentialSlots")
        endpoint = provider["endpoint"]
        if not isinstance(endpoint, dict) or endpoint.get("customPolicy") not in {"allowed", "forbidden", "required"}:
            fail(f"{key} endpoint customPolicy is invalid")
        default_endpoint = endpoint.get("default", "")
        if default_endpoint and not default_endpoint.startswith("https://"):
            fail(f"{key} default endpoint must use HTTPS")
        safe = provider.get("safeRefresh")
        if not isinstance(safe, dict) or safe.get("method") != "GET" or safe.get("readOnly") is not True:
            fail(f"{key} must declare a read-only GET safeRefresh")
        if not safe.get("path") and not safe.get("paths"):
            fail(f"{key} safeRefresh must declare path or paths")
        maximum_request_budget = 64 if provider["adapterType"] == "anthropic_usage_cost" else 20
        if (
            not isinstance(safe.get("requestBudget"), int)
            or not 1 <= safe["requestBudget"] <= maximum_request_budget
        ):
            fail(f"{key} safeRefresh requestBudget must be 1..{maximum_request_budget}")
        if not isinstance(safe.get("minimumIntervalSeconds"), int) or safe["minimumIntervalSeconds"] < 60:
            fail(f"{key} safeRefresh minimumIntervalSeconds must be at least 60")
        expiry = require_iso_date(str(provider["reviewExpiresAt"]), f"{key} reviewExpiresAt")
        if expiry < date.today():
            fail(f"{key} review metadata expired on {expiry}")
        for config_field, config_key in provider["config"].items():
            if config_field != "key" and config_key not in config_keys:
                fail(f"{key} config.{config_field} references missing KConfig key {config_key}")
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
            for source_ref in model["sourceRefs"]:
                if not source_ref["url"].startswith("https://"):
                    fail(f"{key}/{model_id} official source URL must use HTTPS")
            lifecycle = model.get("lifecycle")
            if not isinstance(lifecycle, dict) or lifecycle.get("status") not in {"active", "deprecated", "retired"}:
                fail(f"{key}/{model_id} lifecycle status is invalid")
            if lifecycle.get("status") in {"deprecated", "retired"} and not lifecycle.get("replacementId"):
                fail(f"{key}/{model_id} lifecycle replacementId missing")

            pricing = model.get("pricing")
            if not isinstance(pricing, dict):
                fail(f"{key}/{model_id} missing pricing")
            if "precision" not in pricing:
                fail(f"{key}/{model_id} pricing.precision missing")
            if pricing.get("precision") not in {"official_exact", "official_range", "derived", "unknown"}:
                fail(f"{key}/{model_id} pricing.precision is not a v5 precision value")

            status = pricing.get("status")
            unit = pricing.get("unit")
            if unit in TOKEN_UNITS:
                if "input" not in pricing or "output" not in pricing:
                    fail(f"{key}/{model_id} token pricing must include input and output")
                for field in ("input", "output", "cachedInput"):
                    if field in pricing and pricing[field] is not None and pricing[field] < 0:
                        fail(f"{key}/{model_id} {field} must be non-negative")
                if "contextTiers" in pricing and not isinstance(pricing["contextTiers"], list):
                    fail(f"{key}/{model_id} pricing.contextTiers must be a list")
                if "modalityRates" in pricing and not isinstance(pricing["modalityRates"], dict):
                    fail(f"{key}/{model_id} pricing.modalityRates must be an object")
                if "additiveFees" in pricing and not isinstance(pricing["additiveFees"], list):
                    fail(f"{key}/{model_id} pricing.additiveFees must be a list")
                if "batchDiscountPercent" in pricing and not 0 <= pricing["batchDiscountPercent"] <= 100:
                    fail(f"{key}/{model_id} pricing.batchDiscountPercent must be 0..100")
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
