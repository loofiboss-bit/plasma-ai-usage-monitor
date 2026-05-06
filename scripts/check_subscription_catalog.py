#!/usr/bin/env python3
import argparse
import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "package/contents/catalog/subscriptions-v1.json"
EXPECTED_TOOLS = {"claude-code", "codex-cli", "github-copilot", "cursor", "windsurf", "jetbrains-ai"}
EXACT_PRECISIONS = {"official_exact", "browser_sync_actual", "provider_api"}


def fail(message: str) -> None:
    print(f"Subscription catalog check FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def iso_date(value: str, label: str) -> date:
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
        iso_date(str(ref.get("reviewedAt", "")), f"{context} sourceRef reviewedAt")


def check_price(price, context: str) -> None:
    if price is None:
        return
    if not isinstance(price, dict):
        fail(f"{context} price must be an object")
    if "precision" not in price:
        fail(f"{context} price missing precision")
    if "amount" in price:
        for field in ("amount", "rangeMin", "rangeMax"):
            if field in price and price[field] < 0:
                fail(f"{context} price {field} must be non-negative")
        if not price.get("currency") or not price.get("period"):
            fail(f"{context} numeric price missing currency/period")
    elif "rangeMin" in price or "rangeMax" in price:
        if not price.get("currency") or not price.get("period"):
            fail(f"{context} range price missing currency/period")
    elif "status" not in price:
        fail(f"{context} price must include amount, range, or status")


def check_quota_windows(windows, context: str) -> None:
    if not isinstance(windows, list):
        fail(f"{context} quotaWindows must be a list")
    for window in windows:
        if not isinstance(window, dict):
            fail(f"{context} quota window must be an object")
        for field in ("kind", "label", "precision", "source", "visibleByDefault"):
            if field not in window:
                fail(f"{context} quota window missing {field}")
        has_exact_limit = "limit" in window and isinstance(window["limit"], (int, float))
        has_range = "rangeMin" in window or "rangeMax" in window
        if has_exact_limit and window["precision"] not in EXACT_PRECISIONS:
            if window.get("unit") != "usage_multiplier":
                fail(f"{context}/{window['label']} exact numeric limit has non-exact precision")
        if has_range and window["precision"] == "official_exact":
            fail(f"{context}/{window['label']} range must not use official_exact precision")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--allow-manual-review", action="store_true",
                        help="Allow manual review/source conflict items. Strict release checks allow them only because the UI surfaces them.")
    args = parser.parse_args()

    if not CATALOG.exists():
        fail(f"missing {CATALOG}")

    try:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON: {exc}")

    if catalog.get("schemaVersion") != 1:
        fail("schemaVersion must be 1")
    if catalog.get("runtimeScraping") is not False:
        fail("runtimeScraping must be false")

    reviewed_date = iso_date(str(catalog.get("lastReviewed", "")), "lastReviewed")
    if (date.today() - reviewed_date).days > 30:
        fail(f"catalog lastReviewed is stale: {reviewed_date}")

    tools = catalog.get("tools")
    if not isinstance(tools, list):
        fail("tools must be a list")

    seen = set()
    manual_review = 0
    source_conflict = 0
    for tool in tools:
        if not isinstance(tool, dict):
            fail("tool must be an object")
        key = tool.get("key")
        if not key:
            fail("tool missing key")
        if key in seen:
            fail(f"duplicate tool key: {key}")
        seen.add(key)

        for field in ("label", "vendor"):
            if not tool.get(field):
                fail(f"{key} missing {field}")
        require_source_refs(tool.get("sourceRefs"), key)

        if tool.get("needsManualReview") is True:
            manual_review += 1
        if tool.get("sourceConflict") is True:
            source_conflict += 1

        plans = tool.get("plans", [])
        modes = tool.get("billingModes", [])
        if not isinstance(plans, list) or not isinstance(modes, list):
            fail(f"{key} plans/billingModes must be lists")
        if not plans and not modes:
            fail(f"{key} must have plans or billingModes")

        plan_ids = set()
        for plan in plans:
            if not isinstance(plan, dict):
                fail(f"{key} plan must be an object")
            plan_id = plan.get("id")
            if not plan_id or not plan.get("label"):
                fail(f"{key} plan missing id/label")
            if plan_id in plan_ids:
                fail(f"{key} duplicate plan id: {plan_id}")
            plan_ids.add(plan_id)
            check_price(plan.get("price"), f"{key}/{plan_id}")
            check_quota_windows(plan.get("quotaWindows", []), f"{key}/{plan_id}")

        for mode in modes:
            if not isinstance(mode, dict):
                fail(f"{key} billing mode must be an object")
            if not mode.get("id"):
                fail(f"{key} billing mode missing id")
            for date_field in ("validFrom", "validUntil"):
                if date_field in mode:
                    iso_date(str(mode[date_field]), f"{key}/{mode['id']} {date_field}")
            check_quota_windows(mode.get("quotaWindows", []), f"{key}/{mode['id']}")

    missing = EXPECTED_TOOLS - seen
    extra = seen - EXPECTED_TOOLS
    if missing:
        fail(f"missing tools: {', '.join(sorted(missing))}")
    if extra:
        fail(f"unexpected tools: {', '.join(sorted(extra))}")

    suffix = f"; manualReview={manual_review}, sourceConflict={source_conflict}"
    print(f"Subscription catalog check OK: {len(tools)} tools, reviewed {catalog['lastReviewed']}{suffix}")


if __name__ == "__main__":
    main()
