#!/usr/bin/env python3
"""Audit the signed provider catalog without scraping providers at runtime.

The default check is deterministic and local.  ``--network`` performs bounded
HTTPS reachability checks against the catalog's declared official sources; it
never imports remote values into the catalog.
"""

from __future__ import annotations

import argparse
from datetime import date, datetime, timezone
import json
from pathlib import Path
import sys
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT / "package/contents/catalog/providers-v4.json"
PRECISIONS = {"official_exact", "official_range", "derived", "unknown", "unavailable"}
LIFECYCLE = {"active", "deprecated", "retired"}
UNITS = {"1M_tokens", "generation", "image", "video_second", "credit", "request", "unknown", "not_applicable"}
TRANSIENT_NETWORK_STATUSES = {408, 425, 429, 500, 502, 503, 504}


def iso_date(value: Any, label: str, errors: list[str]) -> date | None:
    try:
        return date.fromisoformat(str(value))
    except (TypeError, ValueError):
        errors.append(f"{label} must be YYYY-MM-DD")
        return None


def iso_timestamp(value: Any, label: str, errors: list[str]) -> datetime | None:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
    except (TypeError, ValueError):
        errors.append(f"{label} must be an ISO-8601 timestamp")
        return None
    if parsed.tzinfo is None:
        errors.append(f"{label} must include a timezone")
        return None
    return parsed.astimezone(timezone.utc)


def add_review(
    items: list[dict[str, Any]],
    label: str,
    reason: str,
    *,
    category: str = "manual",
    actionable: bool = False,
) -> dict[str, Any]:
    item = {
        "label": label,
        "reason": reason,
        "category": category,
        "actionable": actionable,
    }
    items.append(item)
    return item


def network_review_is_actionable(result: str) -> bool:
    """Treat durable HTTP failures as actionable and transport failures as warnings.

    The scheduled check is deliberately best-effort: a provider can be slow or
    temporarily unreachable without its catalog entry being wrong. Explicit
    HTTP failures such as 404 or 410 still indicate that a declared source
    needs maintainer attention.
    """
    if not result.startswith("HTTP "):
        return False
    try:
        status = int(result.split(maxsplit=1)[1])
    except (IndexError, ValueError):
        return False
    return status not in TRANSIENT_NETWORK_STATUSES


def check_source(url: str) -> str:
    request = Request(url, headers={"User-Agent": "plasma-ai-usage-monitor-catalog-audit/19"})
    try:
        with urlopen(request, timeout=5) as response:
            response.read(1)
            return f"HTTP {response.status}"
    except HTTPError as error:
        return f"HTTP {error.code}"
    except (URLError, TimeoutError, OSError) as error:
        return type(error).__name__


def audit(catalog_path: Path, network: bool) -> dict[str, Any]:
    errors: list[str] = []
    reviews: list[dict[str, Any]] = []
    actionable_reviews: list[dict[str, Any]] = []
    network_results: dict[str, str] = {}

    def record_review(
        label: str,
        reason: str,
        *,
        category: str,
        actionable: bool,
    ) -> None:
        item = add_review(reviews, label, reason, category=category, actionable=actionable)
        if item["actionable"]:
            actionable_reviews.append(item)

    try:
        catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return {
            "status": "error",
            "errors": [f"cannot read catalog: {error}"],
            "reviewItems": [],
            "actionableReviewItems": [],
        }

    if not isinstance(catalog, dict):
        return {
            "status": "error",
            "errors": ["catalog root must be an object"],
            "reviewItems": [],
            "actionableReviewItems": [],
        }

    if catalog.get("schemaVersion") != 7:
        errors.append("schemaVersion must be 7")
    if catalog.get("runtimeScraping") is not False:
        errors.append("runtimeScraping must be false")
    if not isinstance(catalog.get("sequence"), int) or catalog["sequence"] < 1:
        errors.append("sequence must be a positive integer")
    slo = catalog.get("freshnessSloDays")
    if not isinstance(slo, int) or slo < 1:
        errors.append("freshnessSloDays must be a positive integer")
        slo = 30

    today = date.today()
    reviewed = iso_date(catalog.get("lastReviewed"), "lastReviewed", errors)
    if reviewed:
        age = (today - reviewed).days
        if age > slo:
            errors.append(f"catalog lastReviewed is {age} days old; SLO is {slo} days")
    expiry = iso_timestamp(catalog.get("hardExpiresAt"), "hardExpiresAt", errors)
    if expiry and expiry <= datetime.now(timezone.utc):
        errors.append("hardExpiresAt is expired")
    if catalog.get("verificationState") not in {"packaged", "remote_verified"}:
        errors.append("verificationState must be packaged or remote_verified")
    if catalog.get("estimatesAllowed") is not True:
        record_review(
            "catalog",
            "estimatesAllowed is false; cost estimates remain unavailable",
            category="catalog_policy",
            actionable=True,
        )

    providers = catalog.get("providers")
    if not isinstance(providers, list) or not providers:
        errors.append("providers must be a non-empty list")
        providers = []

    provider_keys: set[str] = set()
    model_count = 0
    stale_source_count = 0
    unknown_price_count = 0
    seen_source_urls: set[str] = set()

    for provider in providers:
        if not isinstance(provider, dict):
            errors.append("provider must be an object")
            continue
        key = str(provider.get("key", ""))
        if not key:
            errors.append("provider key is missing")
            continue
        if key in provider_keys:
            errors.append(f"duplicate provider key: {key}")
        provider_keys.add(key)
        if not provider.get("label"):
            errors.append(f"{key} label is missing")
        provider_expiry = iso_date(provider.get("reviewExpiresAt"), f"{key}.reviewExpiresAt", errors)
        if provider_expiry and provider_expiry < today:
            errors.append(f"{key}.reviewExpiresAt is expired")
        models = provider.get("models")
        if not isinstance(models, list) or not models:
            errors.append(f"{key}.models must be a non-empty list")
            continue

        model_ids: set[str] = set()
        alias_ids: set[str] = set()
        for model in models:
            model_count += 1
            if not isinstance(model, dict):
                errors.append(f"{key} model must be an object")
                continue
            model_id = str(model.get("id", ""))
            context = f"{key}/{model_id or '<missing>'}"
            if not model_id:
                errors.append(f"{key} model id is missing")
            elif model_id in model_ids:
                errors.append(f"duplicate model id: {context}")
            model_ids.add(model_id)

            aliases = model.get("aliases", [])
            if not isinstance(aliases, list) or any(not isinstance(alias, str) or not alias.strip() for alias in aliases):
                errors.append(f"{context}.aliases must be a list of non-empty exact IDs")
            else:
                for alias in aliases:
                    if alias in alias_ids or alias in model_ids or "*" in alias:
                        errors.append(f"{context} contains a non-unique or prefix alias: {alias}")
                    alias_ids.add(alias)

            lifecycle = model.get("lifecycle")
            if not isinstance(lifecycle, dict) or lifecycle.get("status") not in LIFECYCLE:
                errors.append(f"{context}.lifecycle.status is invalid")
            elif lifecycle["status"] in {"deprecated", "retired"}:
                replacement = lifecycle.get("replacementId")
                if not isinstance(replacement, str) or not replacement:
                    errors.append(f"{context} needs lifecycle.replacementId")
                else:
                    record_review(
                        context,
                        f"lifecycle is {lifecycle['status']}; replacement is {replacement}",
                        category="lifecycle",
                        actionable=False,
                    )

            source_refs = model.get("sourceRefs")
            if not isinstance(source_refs, list) or not source_refs:
                errors.append(f"{context}.sourceRefs is missing")
                source_refs = []
            for source in source_refs:
                if not isinstance(source, dict):
                    errors.append(f"{context}.sourceRefs entry must be an object")
                    continue
                url = str(source.get("url", ""))
                if not url.startswith("https://"):
                    errors.append(f"{context} source URL must use HTTPS")
                source_reviewed = iso_date(source.get("reviewedAt"), f"{context}.sourceRefs.reviewedAt", errors)
                if source_reviewed:
                    source_age = (today - source_reviewed).days
                    if source_age > slo:
                        stale_source_count += 1
                        record_review(
                            context,
                            f"official source review is {source_age} days old",
                            category="stale_source",
                            actionable=True,
                        )
                if network and url and url not in seen_source_urls:
                    seen_source_urls.add(url)
                    network_results[url] = check_source(url)
                    if not network_results[url].startswith("HTTP 2") and not network_results[url].startswith("HTTP 3"):
                        record_review(
                            url,
                            f"official source reachability check returned {network_results[url]}",
                            category="source_reachability",
                            actionable=network_review_is_actionable(network_results[url]),
                        )

            pricing = model.get("pricing")
            if not isinstance(pricing, dict):
                errors.append(f"{context}.pricing is missing")
                continue
            precision = pricing.get("precision")
            if precision not in PRECISIONS:
                errors.append(f"{context}.pricing.precision is invalid")
            status = pricing.get("status")
            unit = pricing.get("unit")
            if status in {"unknown", "not_applicable"} or precision in {"unknown", "unavailable"}:
                unknown_price_count += 1
                record_review(
                    context,
                    f"pricing is {status or precision}; estimate is unavailable",
                    category="pricing_unavailable",
                    actionable=False,
                )
            elif unit not in UNITS:
                errors.append(f"{context}.pricing.unit is invalid")
            elif unit == "1M_tokens":
                for field in ("input", "output"):
                    value = pricing.get(field)
                    if not isinstance(value, (int, float)) or value < 0:
                        errors.append(f"{context}.pricing.{field} must be non-negative")

            change = model.get("priceChange")
            if change is not None:
                if not isinstance(change, dict):
                    errors.append(f"{context}.priceChange must be an object")
                else:
                    iso_date(change.get("effectiveDate"), f"{context}.priceChange.effectiveDate", errors)
                    if not change.get("summary") or not isinstance(change.get("previous"), dict) or not isinstance(change.get("current"), dict):
                        errors.append(f"{context}.priceChange needs summary, previous and current values")

        overlap = model_ids & alias_ids
        if overlap:
            errors.append(f"{key} aliases collide with model IDs: {', '.join(sorted(overlap))}")

    result: dict[str, Any] = {
        "status": "error" if errors else (
            "review_required" if actionable_reviews else "expected_review" if reviews else "ok"
        ),
        "catalog": str(catalog_path.relative_to(ROOT) if catalog_path.is_relative_to(ROOT) else catalog_path),
        "schemaVersion": catalog.get("schemaVersion"),
        "catalogVersion": catalog.get("catalogVersion"),
        "sequence": catalog.get("sequence"),
        "lastReviewed": catalog.get("lastReviewed"),
        "hardExpiresAt": catalog.get("hardExpiresAt"),
        "stats": {
            "providers": len(provider_keys),
            "models": model_count,
            "staleSourceReviews": stale_source_count,
            "unknownOrUnavailablePricing": unknown_price_count,
            "networkSourcesChecked": len(network_results),
            "actionableReviewItems": len(actionable_reviews),
            "expectedReviewItems": len(reviews) - len(actionable_reviews),
        },
        "errors": errors,
        "reviewItems": reviews,
        "actionableReviewItems": actionable_reviews,
        "network": network_results,
    }
    return result


def markdown(report: dict[str, Any]) -> str:
    lines = [
        f"# Provider catalog drift report ({report.get('status', 'error')})",
        "",
        f"- Catalog: `{report.get('catalog', '')}`",
        f"- Version: `{report.get('catalogVersion', '')}` / schema `{report.get('schemaVersion', '')}`",
        f"- Sequence: `{report.get('sequence', '')}`",
        f"- Last reviewed: `{report.get('lastReviewed', '')}`",
        f"- Hard expiry: `{report.get('hardExpiresAt', '')}`",
        "",
        "## Stats",
        "",
    ]
    for key, value in report.get("stats", {}).items():
        lines.append(f"- {key}: {value}")
    for heading, key in (("Errors", "errors"), ("Review items", "reviewItems")):
        lines.extend(["", f"## {heading}", ""])
        values = report.get(key, [])
        if not values:
            lines.append("- None")
        elif key == "errors":
            lines.extend(f"- {value}" for value in values)
        else:
            lines.extend(
                f"- [{'actionable' if value.get('actionable') else 'expected'}] "
                f"`{value['label']}`: {value['reason']}"
                for value in values
            )
    if report.get("network"):
        lines.extend(["", "## Network checks", ""])
        lines.extend(f"- `{url}`: {result}" for url, result in report["network"].items())
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--network", action="store_true")
    parser.add_argument("--strict-review", action="store_true", help="return 2 when maintainer review is needed")
    parser.add_argument(
        "--strict-actionable-review",
        action="store_true",
        help="return 2 only when an actionable maintainer review is needed",
    )
    parser.add_argument("--format", choices=("text", "json", "markdown"), default="text")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    report = audit(args.catalog.resolve(), args.network)
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n" if args.format == "json" else markdown(report) if args.format == "markdown" else (
        f"Catalog drift {report['status']}: {report.get('stats', {})}; "
        f"errors={len(report.get('errors', []))}, reviews={len(report.get('reviewItems', []))}, "
        f"actionable={len(report.get('actionableReviewItems', []))}"
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="" if rendered.endswith("\n") else "\n")

    if report.get("errors"):
        return 1
    if args.strict_review and report.get("reviewItems"):
        return 2
    if args.strict_actionable_review and report.get("actionableReviewItems"):
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
