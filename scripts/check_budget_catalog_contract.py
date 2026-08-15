#!/usr/bin/env python3
"""Validate the catalog-driven budget capability boundaries."""

from __future__ import annotations

import json
import re
import sys
import xml.etree.ElementTree as ET
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "package/contents/catalog/providers-v4.json"
UI = ROOT / "package/contents/ui"


def fail(message: str) -> None:
    print(f"Budget catalog contract FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    if catalog.get("schemaVersion") != 7:
        fail("provider catalog must use schema v7")

    config_keys = {
        node.attrib["name"]
        for node in ET.parse(ROOT / "package/contents/config/main.xml").iter()
        if node.tag.endswith("entry") and "name" in node.attrib
    }
    allowed_scopes = {
        "aggregate", "project", "workspace", "model", "service_tier", "line_item"
    }
    allowed_cycles = {
        "calendar_day", "iso_week", "calendar_month", "anchored_month", "provider_reset"
    }
    budget_sources: list[str] = []
    declared_scopes: set[str] = set()
    declared_cycles: set[str] = set()
    for provider in catalog.get("providers", []):
        key = provider.get("key", "<unknown>")
        for field, config_key in provider.get("config", {}).items():
            if field != "key" and config_key not in config_keys:
                fail(f"{key} config.{field} does not resolve to KConfig")
        if provider.get("budgetPolicyContractVersion") != "budget-policy-v2":
            continue
        budget_sources.append(key)
        scopes = provider.get("supportedBudgetScopes", [])
        cycles = provider.get("supportedBillingCycles", [])
        if not scopes or not set(scopes) <= allowed_scopes:
            fail(f"{key} has invalid supportedBudgetScopes")
        if not cycles or not set(cycles) <= allowed_cycles:
            fail(f"{key} has invalid supportedBillingCycles")
        declared_scopes.update(scopes)
        declared_cycles.update(cycles)
        try:
            reviewed = date.fromisoformat(provider["capabilityReviewedAt"])
            expires = date.fromisoformat(provider["capabilityReviewExpiresAt"])
        except (KeyError, TypeError, ValueError):
            fail(f"{key} lacks valid budget capability review dates")
        if expires < date.today() or expires < reviewed:
            fail(f"{key} has expired budget capability metadata")

    if sorted(budget_sources) != ["anthropic", "litellm", "openai", "openrouter"]:
        fail(f"unexpected reviewed budget inventory: {', '.join(sorted(budget_sources))}")

    observation = (ROOT / "plugin/budgetobservationquery.cpp").read_text(encoding="utf-8")
    resolver = (ROOT / "plugin/billingcycleresolver.cpp").read_text(encoding="utf-8")
    for scope in declared_scopes - {"aggregate"}:
        if f'QLatin1String("{scope}")' not in observation:
            fail(f"catalog scope has no dynamic observation resolver: {scope}")
    for cycle in declared_cycles:
        if f'QLatin1String("{cycle}")' not in resolver:
            fail(f"catalog cycle has no BillingCycleResolver path: {cycle}")

    registry = (UI / "ProviderRegistry.qml").read_text(encoding="utf-8")
    coordinator = (UI / "RuntimeCoordinator.qml").read_text(encoding="utf-8")
    monitor = (UI / "NativeMonitor.qml").read_text(encoding="utf-8")
    alerts = (UI / "configAlerts.qml").read_text(encoding="utf-8")
    runtime = (UI / "BudgetPolicyRuntime.qml").read_text(encoding="utf-8")
    migration = (UI / "BudgetPolicyMigration.qml").read_text(encoding="utf-8")
    provider_catalog = (UI / "ProviderCatalog.qml").read_text(encoding="utf-8")

    for name, text in (("ProviderRegistry", registry), ("RuntimeCoordinator", coordinator)):
        if re.search(r"(?:daily|monthly)Budget(?:Config)?Key", text, re.I):
            fail(f"{name} contains legacy budget runtime inventory")
    if "budgetPolicyRepository" in coordinator or "catalogSupportedScopes" in coordinator:
        fail("RuntimeCoordinator still owns policy repository/catalog wiring")
    if "migrateLegacyBudgets" in monitor or "DailyBudget" in monitor or "MonthlyBudget" in monitor:
        fail("NativeMonitor still owns legacy budget migration wiring")
    if not all(token in runtime for token in (
        "budgetPolicyContractVersion", "supportedBudgetScopes",
        "supportedBillingCycles", "validatedBudgetScopes",
    )):
        fail("BudgetPolicyRuntime lacks catalog-driven query metadata")
    if "legacyBudgetProviders" not in provider_catalog or "migrateLegacyBudgets" not in migration:
        fail("legacy KConfig migration is not isolated to its one-shot component")
    if re.search(r"\bcfg_[a-z0-9]+NotificationsEnabled\b", alerts):
        fail("provider notification inventory is hardcoded in configAlerts")
    for token in ("providerCatalog.providers", "notificationsConfigKey", "providerNotificationDraft"):
        if token not in alerts:
            fail(f"provider notifications are not catalog-driven: {token}")

    facade = (ROOT / "plugin/runwayquery.cpp").read_text(encoding="utf-8")
    quota = (ROOT / "plugin/quotarunwayquery.cpp").read_text(encoding="utf-8")
    if re.search(r"ForecastContract::Result\s+QuotaRunwayQuery::evaluate", facade) or "medianConsumptionSlope" in facade:
        fail("quota implementation remains in RunwayQuery compatibility facade")
    if "QuotaRunwayQuery::evaluate" not in quota or "consumptionSlope" not in quota:
        fail("QuotaRunwayQuery does not own its focused implementation")

    print(
        "Budget catalog contract OK: "
        f"{len(budget_sources)} reviewed sources, {len(declared_scopes)} scopes, "
        f"{len(declared_cycles)} cycles, dynamic notifications and split quota engine"
    )


if __name__ == "__main__":
    main()
