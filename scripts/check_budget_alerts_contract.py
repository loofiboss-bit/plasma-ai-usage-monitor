#!/usr/bin/env python3
"""Validate transition persistence, integration privacy, and read-only bounds."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    raise SystemExit(f"Budget alerts contract FAIL: {message}")


def require(text: str, label: str, *tokens: str) -> None:
    for token in tokens:
        if token not in text:
            fail(f"{label} is missing {token!r}")


def main() -> None:
    repository = (ROOT / "plugin" / "budgetpolicyrepository.cpp").read_text(
        encoding="utf-8"
    )
    require(
        repository,
        "budgetpolicyrepository.cpp",
        "prepareTransitions",
        "m_database.transaction()",
        "INSERT INTO budget_policy_events",
        "INSERT INTO budget_policy_state",
        "period_reset",
        "delivery_status='pending'",
        "markEventDelivered",
        "markEventFailed",
    )
    if repository.index("INSERT INTO budget_policy_events") > repository.index(
        "m_database.commit()", repository.index("prepareTransitions")
    ):
        fail("events must be persisted before the transition transaction commits")

    controller = (ROOT / "package" / "contents" / "ui" / "NotificationController.qml").read_text(
        encoding="utf-8"
    )
    require(
        controller,
        "NotificationController.qml",
        "budgetPolicyRepository.prepareTransitions",
        "budgetPolicyRepository.markEventDelivered",
        "budgetPolicyRepository.markEventFailed",
        'return "dnd"',
        '? "cooldown" : ""',
        "policyRequested(string policyId)",
        'i18n("Open Budget Control")',
    )
    policy_block = controller[
        controller.index("function processBudgetPolicy") : controller.index(
            "function processGuardrail", controller.index("function processBudgetPolicy")
        )
    ]
    for forbidden in (
        "refresh",
        "inference",
        "completion",
        "setModel",
        "setProvider",
        "apiKey",
    ):
        if forbidden in policy_block:
            fail(f"budget policy alert path is not read-only: found {forbidden!r}")

    webhook = (ROOT / "plugin" / "webhooknotifier.cpp").read_text(encoding="utf-8")
    require(
        webhook,
        "webhooknotifier.cpp",
        "The external snapshot is intentionally limited to five bounded",
        'QStringLiteral("providerDisplayName")',
        'QStringLiteral("percentClass")',
        'QStringLiteral("linkText")',
        'QStringLiteral("budget-policy-transition-%1")',
    )

    runtime = (ROOT / "package" / "contents" / "ui" / "RuntimeCoordinator.qml").read_text(
        encoding="utf-8"
    )
    metric_start = runtime.index("function appendGuardrailMetrics")
    metric_end = runtime.index("function labelValue", metric_start)
    metric_block = runtime[metric_start:metric_end]
    require(
        metric_block,
        "guardrail metric block",
        'var labels = "source=\\"',
        ',kind=\\"',
        ',value_class=\\"',
        "exceeded: 4",
    )
    for forbidden_label in ("policy", "scope", "project", "workspace", "api_key"):
        if f'{forbidden_label}=\\"' in metric_block:
            fail(f"Prometheus exposes forbidden label {forbidden_label!r}")

    diagnostics = (ROOT / "package" / "contents" / "ui" / "configDiagnostics.qml").read_text(
        encoding="utf-8"
    )
    if '"budgetPolicySelectionRequest"' in diagnostics:
        fail("the local policy selection request must not be portable or diagnostic")

    print(
        "Budget alerts contract OK: persist-before-delivery, transition-only state, "
        "strict webhook allowlist, bounded metrics, and zero provider mutation path"
    )


if __name__ == "__main__":
    main()
