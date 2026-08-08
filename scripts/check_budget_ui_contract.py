#!/usr/bin/env python3
"""Validate Budget Control staging, lazy popup work, and privacy-safe UI contracts."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "package" / "contents" / "ui"


def fail(message: str) -> None:
    raise SystemExit(f"Budget UI contract FAIL: {message}")


def require(text: str, label: str, *tokens: str) -> None:
    for token in tokens:
        if token not in text:
            fail(f"{label} is missing {token!r}")


def main() -> None:
    representation = (UI / "FullRepresentation.qml").read_text(encoding="utf-8")
    popup = (UI / "PopupRepresentation.qml").read_text(encoding="utf-8")
    native_monitor = (UI / "NativeMonitor.qml").read_text(encoding="utf-8")
    require(
        popup,
        "PopupRepresentation.qml",
        "asynchronous: true",
        "interval: 500",
        "running: !!popupRoot.monitor.popupExpanded",
        'Qt.resolvedUrl("FullRepresentation.qml")',
        '{ "monitor": popupRoot.monitor }',
        'i18n("Loading Overview…")',
    )
    require(
        native_monitor,
        "NativeMonitor.qml",
        "fullRepresentationComponent: PopupRepresentation",
        'readonly property bool popupExpanded: !!Plasmoid["expanded"]',
    )
    tab_start = representation.index("Kirigami.NavigationTabBar")
    tab_end = representation.index("QQC2.ToolBar", tab_start)
    tab_block = representation[tab_start:tab_end]
    labels = re.findall(r'text:\s*i18n\("([^"]+)"\)', tab_block)
    if labels != ["Overview", "History", "Insights"]:
        fail(f"popup tabs must be exactly Overview, History, Insights; got {labels}")
    if representation.count("Loader {") != 2:
        fail("popup destinations and onboarding must use two lazy Loaders")
    for path in UI.rglob("*.qml"):
        text = path.read_text(encoding="utf-8")
        if re.search(r'i18n\("[^"]*Analyst', text):
            fail(f"{path.relative_to(ROOT)} exposes the retired Analyst name")
    require(
        representation,
        "FullRepresentation.qml",
        'fullRoot.destination === 0 ? "views/OverviewView.qml"',
        'fullRoot.destination === 1 ? "views/HistoryView.qml"',
        ': "views/AnalystView.qml"',
        "asynchronous: true",
        "active: fullRoot.showGuidedSetup",
    )

    detail = (UI / "views" / "SourceDetailView.qml").read_text(encoding="utf-8")
    require(
        detail,
        "SourceDetailView.qml",
        "return showAllScopes ? rows : rows.slice(0, 8)",
        "onClicked: detail.showAllScopes = true",
        'i18n("Unattributed scope")',
        'objectName: "sourceDetailHistory"',
    )
    runway = (UI / "RunwayCard.qml").read_text(encoding="utf-8")
    require(
        runway,
        "RunwayCard.qml",
        "previousPeriodSpentMinor",
        'i18n("Spent")',
        'i18n("Remaining")',
        'i18n("Safe today")',
    )

    editor = (UI / "components" / "BudgetPolicyEditor.qml").read_text(
        encoding="utf-8"
    )
    store = (UI / "components" / "BudgetPolicyDraftStore.qml").read_text(
        encoding="utf-8"
    )
    require(
        editor,
        "BudgetPolicyEditor.qml",
        "supportedBudgetScopes",
        "supportedBillingCycles",
        'Accessible.name: i18n("Budget policy editor")',
        'objectName: "budgetLimitField"',
        "function commitLimitText()",
        'i18n("Live threshold preview")',
        "activeFocusOnTab: true",
    )
    if re.search(r"#[0-9A-Fa-f]{3,8}\b", editor):
        fail("BudgetPolicyEditor.qml must use semantic KDE theme colors")
    require(
        store,
        "BudgetPolicyDraftStore.qml",
        "function apply()",
        "repository.replacePolicies(payload)",
        "function discard()",
        "Component.onDestruction",
    )

    smoke = (ROOT / "scripts" / "smoke_test_plasmoid.sh").read_text(
        encoding="utf-8"
    )
    require(
        smoke,
        "smoke_test_plasmoid.sh",
        "1.25 1.4 1.5 2",
        "overview source-detail history analyst onboarding settings",
    )

    print(
        "Budget UI contract OK: three lazy popup tabs, eight-row scope gate, "
        "staged editor, semantic theme colors, and 100/125/140/150/200% coverage"
    )


if __name__ == "__main__":
    main()
