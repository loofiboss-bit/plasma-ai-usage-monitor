#!/usr/bin/env python3
"""Validate the non-visual accessibility contract for daily surfaces."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str, *tokens: str) -> list[str]:
    text = read(relative)
    return [
        f"{relative}: missing {token!r}"
        for token in tokens
        if token not in text
    ]


def main() -> None:
    errors: list[str] = []
    errors += require(
        "package/contents/ui/CompactRepresentation.qml",
        "Accessible.role: Accessible.Button",
        "Accessible.name: i18n(\"AI Usage Monitor: %1\", accessibleText())",
        "Accessible.onPressAction: Plasmoid.activated()",
        'objectName: "compactSeveritySymbol"',
        'text: "!"',
    )
    errors += require(
        "package/contents/ui/main.qml",
        'Accessible.onPressAction: root.plasmoid["activated"]()',
    )
    errors += require(
        "package/contents/ui/components/CompactMetricState.qml",
        'i18n("Critical: %1", urgent.displayName)',
        'i18n("Warning: %1", urgent.displayName)',
        "summary.mostUrgentSource",
    )
    errors += require(
        "package/contents/ui/DailySourceCard.qml",
        "Accessible.name:",
        "attentionText()",
        'i18n("Critical · ")',
        'i18n("Warning · ")',
        "activeFocusOnTab: true",
    )
    errors += require(
        "package/contents/ui/MultiSeriesChart.qml",
        "Accessible.role: Accessible.Graphic",
        "Accessible.name: accessibleSummary()",
        "recorded points",
        "gaps",
    )
    errors += require(
        "package/contents/ui/FullRepresentation.qml",
        "Kirigami.NavigationTabBar",
        'Accessible.name: i18n("Monitor actions")',
        'Accessible.name: i18n("Refresh all configured sources")',
        'Accessible.name: i18n("Run guided setup again")',
        'Accessible.name: i18n("Configure AI Usage Monitor")',
        "returnFocusSourceId",
        "restoreSourceFocus(sourceId)",
    )
    errors += require(
        "package/contents/ui/components/DailyFocus.qml",
        "Accessible.role: Accessible.Grouping",
        'objectName: "dailyFocusAction"',
        "activeFocusOnTab: true",
    )
    errors += require(
        "package/contents/ui/views/SourceDetailView.qml",
        'Accessible.name: i18n("Back to source list")',
        'objectName: "sourceDetailPrimaryAction"',
        'objectName: "sourceDetailSettings"',
        'objectName: "sourceDetailHistory"',
        "activeFocusOnTab: true",
        'Accessible.name: i18n("Metric provenance: %1", text)',
    )
    errors += require(
        "package/contents/ui/AnalystTab.qml",
        'i18n("Insights view loading")',
        'i18n("Insights view ready")',
    )
    errors += require(
        "package/contents/ui/RuntimeCoordinator.qml",
        "property bool startupRefreshCompleted: false",
        "runtime.startupRefreshCompleted = true",
        "function refreshCredentialProviders(reason)",
        "runtime.refreshCredentialProviders(",
    )

    history = read("package/contents/ui/views/HistoryView.qml")
    required_history_names = (
        "History view loading",
        "History view ready",
        "Show comparison history",
        "Refresh history",
        "History range",
        "History source",
        "History metric",
        "Export history to a file",
        "Copy history as CSV",
    )
    for name in required_history_names:
        if name not in history:
            errors.append(
                "package/contents/ui/views/HistoryView.qml: "
                f"missing accessible control name {name!r}"
            )
    if history.count("activeFocusOnTab: true") < 7:
        errors.append(
            "package/contents/ui/views/HistoryView.qml: every primary history "
            "control must be keyboard focusable"
        )

    if errors:
        raise SystemExit("\n".join(errors))
    print(
        "Daily UI accessibility contract passed: keyboard focus, named actions, "
        "source-aware severity, and non-color chart/status summaries."
    )


if __name__ == "__main__":
    main()
