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
        destruction_hooks = re.findall(
            r"Component\.onDestruction\s*:\s*(.*?)(?=\n\s*(?:Component\.|[A-Z][A-Za-z]+\s*\{|$))",
            text,
            re.S,
        )
        for hook in destruction_hooks:
            if re.search(r"\b(?:commit|storeKey|removeKey)\s*\(", hook):
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
    if "Qt.resolvedUrl" in text:
        fail("Diagnostics actions must not depend on repository-relative files")
    for token in (
        "AppInfo.systemDiagnostics(frontendVersion)",
        "AppInfo.databaseDiagnostics()",
        "AppInfo.buildSupportReport",
        "diagnosticsSourceSnapshot",
        "providerGuideUrl",
        "providerCatalogUrl",
        "subscriptionGuideUrl",
    ):
        if token not in text:
            fail(f"Diagnostics is missing the native recovery contract: {token}")

    bootstrap = (UI / "DependencyBootstrap.qml").read_text(encoding="utf-8")
    controller = (UI / "DependencyBootstrapController.qml").read_text(encoding="utf-8")
    if "required property string supportReport" not in bootstrap or "Copy report" not in bootstrap:
        fail("Missing-plugin recovery does not expose a copyable support report")
    if "function supportReport()" not in controller or "Native status:" not in controller:
        fail("Missing-plugin support report does not classify the bootstrap state")


def validate_source_choice_settings() -> None:
    providers = (UI / "configProviders.qml").read_text(encoding="utf-8")
    source_list = (UI / "ProviderSourceList.qml").read_text(encoding="utf-8")
    details = (UI / "ProviderSourceDetails.qml").read_text(encoding="utf-8")
    general = (UI / "configGeneral.qml").read_text(encoding="utf-8")
    onboarding = (UI / "onboarding" / "SetupConfigureStep.qml").read_text(encoding="utf-8")

    if providers.count("ProviderSourceDetails {") != 1 or "ProviderSourceList {" not in providers:
        fail("Providers Settings must render one catalog-driven master/detail surface")
    if "ProviderSettingsController" not in providers or "detectedLocalSources" not in providers:
        fail("Providers Settings must include deterministic selection and detected local sources")
    for token in ("activeFocusOnTab: true", "Keys.onUpPressed", "Keys.onDownPressed"):
        if token not in source_list:
            fail(f"Provider source list is missing its keyboard contract: {token}")
    if "CredentialEditor" not in details or "CredentialEditor" not in onboarding:
        fail("Settings and onboarding must share the credential editor")
    if "Local-First" in general or "presetCombo" in general:
        fail("General Settings still contains misleading presets")
    if "enabledProviders" not in general or "cfg_advancedSettingsMode ? generalPage.enabledProviders" not in general:
        fail("per-source intervals must be advanced-only and limited to enabled sources")


def validate_budget_control() -> None:
    page = (UI / "configBudget.qml").read_text(encoding="utf-8")
    store = (UI / "components" / "BudgetPolicyDraftStore.qml").read_text(
        encoding="utf-8"
    )
    editor = (UI / "components" / "BudgetPolicyEditor.qml").read_text(
        encoding="utf-8"
    )
    config_model = (ROOT / "package" / "contents" / "config" / "config.qml").read_text(
        encoding="utf-8"
    )

    if 'name: i18n("Budget Control")' not in config_model:
        fail("the budget settings category must be visibly named Budget Control")
    for token in (
        "BudgetPolicyRepository",
        "BudgetPolicyDraftStore",
        "function saveConfig()",
        "Component.onDestruction: policyDrafts.discard()",
        "deleteDialog.open()",
        "activeFocusOnTab: true",
        "Accessible.name",
    ):
        if token not in page:
            fail(f"Budget Control is missing its staged editor contract: {token}")
    for retired in ("DailyBudget", "MonthlyBudget"):
        if re.search(rf"\bcfg_[A-Za-z0-9_]*{retired}\b", page):
            fail(f"Budget Control still binds the hidden legacy {retired} inventory")
    if "replacePolicies(payload)" not in store or "function apply()" not in store:
        fail("BudgetPolicyDraftStore lacks its atomic Apply boundary")
    if re.search(r"repository\.(?:createPolicy|updatePolicy|deletePolicy|setPolicyEnabled)\s*\(", editor):
        fail("BudgetPolicyEditor writes through the repository during field navigation")
    for token in (
        "supportedBudgetScopes",
        "Live threshold preview",
        "scopeIdentity",
        "warningPercent",
        "criticalPercent",
        "setSnoozedUntilNextPeriod",
    ):
        if token not in editor:
            fail(f"Budget policy editor is missing: {token}")


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
    validate_secret_pages()
    validate_diagnostics()
    validate_source_choice_settings()
    validate_budget_control()

    print(
        f"KCM contract check OK: {len(providers)} providers, staged budget policies, transactional secrets, safe diagnostics"
    )


if __name__ == "__main__":
    main()
