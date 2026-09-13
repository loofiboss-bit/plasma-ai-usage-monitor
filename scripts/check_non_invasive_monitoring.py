#!/usr/bin/env python3
"""Release gate for v13's no-billable-background-traffic invariant."""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def fail(message: str) -> None:
    print(f"Non-invasive monitoring FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)

catalog = json.loads((ROOT / "package/contents/catalog/providers-v4.json").read_text())
for provider in catalog["providers"]:
    safe = provider.get("safeRefresh")
    if safe and (safe.get("method") != "GET" or safe.get("readOnly") is not True):
        fail(f"{provider['key']} scheduled adapter is not read-only GET")
    if provider.get("probePolicy") == "manual_only" and safe:
        fail(f"{provider['key']} manual-only adapter has a scheduled endpoint")

checks = {
    "plugin/openaicompatibleprovider.cpp": ("OpenAICompatibleProvider::refreshImpl", "OpenAICompatibleProvider::testConnectionNow"),
    "plugin/azureopenaiprovider.cpp": ("AzureOpenAIProvider::refreshImpl", "AzureOpenAIProvider::testConnectionNow"),
    "plugin/googleprovider.cpp": ("GoogleProvider::refreshImpl", "GoogleProvider::countTokensDiagnostic"),
    "plugin/anthropicprovider.cpp": ("AnthropicProvider::refreshImpl", "AnthropicProvider::countTokensDiagnostic"),
}
for relative, (scheduled, manual) in checks.items():
    text = (ROOT / relative).read_text()
    start = text.find(scheduled)
    end = text.find(manual, start)
    if start < 0 or end < 0:
        fail(f"cannot identify scheduled/manual boundary in {relative}")
    scheduled_body = text[start:end]
    if re.search(r"networkManager\(\)->(post|put|deleteResource)\s*\(", scheduled_body):
        fail(f"scheduled refresh performs a mutating request in {relative}")
    if "chat/completions" in scheduled_body or ":countTokens" in scheduled_body:
        fail(f"scheduled refresh reaches an inference endpoint in {relative}")

runtime_coordinator = (
    ROOT / "package/contents/ui/RuntimeCoordinator.qml"
).read_text()
prometheus_helper = (
    ROOT / "package/contents/ui/PrometheusMetrics.js"
).read_text()
metrics_start = runtime_coordinator.find("var typedMetrics")
metrics_end = runtime_coordinator.find(
    "ai_usage_provider_probe_input_tokens", metrics_start
)
if metrics_start < 0 or metrics_end < 0:
    fail("cannot identify the Prometheus typed-metric boundary")
metrics_body = runtime_coordinator[metrics_start:metrics_end]
if "aggregationLevel || \"\") === \"scoped\"" not in metrics_body:
    fail("Prometheus export does not explicitly exclude scoped rows")
for forbidden_label in (
    "model_scope",
    "project_scope",
    "service_tier",
    "line_item",
    "api_key_id",
):
    if forbidden_label in metrics_body:
        fail(f"Prometheus export contains high-cardinality label {forbidden_label}")

guardrail_start = runtime_coordinator.find("function appendGuardrailMetrics")
guardrail_end = runtime_coordinator.find("function labelValue", guardrail_start)
if guardrail_start < 0 or guardrail_end < 0:
    fail("cannot identify the Prometheus guardrail-metric boundary")
guardrail_body = runtime_coordinator[guardrail_start:guardrail_end]
for required in (
    "ai_usage_guardrail_risk_state",
    "ai_usage_guardrail_seconds_until_event",
    '"quota_exhaustion"',
    '"budget_overrun"',
    'source=\\""',
    'kind=\\""',
    'value_class=\\""',
):
    if required not in guardrail_body:
        fail(f"Prometheus guardrail export is missing {required}")
for forbidden_dimension in (
    "scope",
    "model",
    "project",
    "workspace",
    "serviceTier",
    "lineItem",
    "apiKey",
    "stableId",
):
    if forbidden_dimension in guardrail_body:
        fail(
            "Prometheus guardrail export contains high-cardinality dimension "
            f"{forbidden_dimension}"
        )
if '"budget_pacing"' in guardrail_body:
    fail("Prometheus guardrail export uses a non-contract risk kind")

for required in (
    "ai_usage_tool_quota_percent_remaining",
    "ai_usage_tool_quota_reset_timestamp_seconds",
    'tool=\\""',
    'kind=\\""',
    'source=\\""',
    'quality=\\""',
):
    if required not in prometheus_helper:
        fail(f"Prometheus tool-quota export is missing {required}")
if "monitor.quotaWindowsChanged.connect(syncMetricsPayload)" not in runtime_coordinator:
    fail("Prometheus payload does not refresh when synchronized quota windows change")
if "function onPayloadRequested()" not in runtime_coordinator:
    fail("Prometheus payload is not regenerated for each scrape")

dashboard = json.loads((ROOT / "docs/grafana-dashboard.json").read_text())
dashboard_expressions = {
    target["expr"]
    for panel in dashboard.get("panels", [])
    for target in panel.get("targets", [])
    if isinstance(target.get("expr"), str)
}
emitted_metric_names = set(re.findall(
    r"\bai_usage_[a-z0-9_]+\b", runtime_coordinator + prometheus_helper
))
queried_metric_names = set(re.findall(
    r"\bai_usage_[a-z0-9_]+\b", "\n".join(dashboard_expressions)
))
unknown_dashboard_metrics = queried_metric_names - emitted_metric_names
if unknown_dashboard_metrics:
    fail(
        "Grafana dashboard queries metrics not emitted by the runtime: "
        + ", ".join(sorted(unknown_dashboard_metrics))
    )
required_dashboard_queries = {
    'min(ai_usage_tool_quota_percent_remaining{tool=~"$tool"})',
    'clamp_min(min(ai_usage_tool_quota_reset_timestamp_seconds{tool=~"$tool"}) - time(), 0)',
    'ai_usage_tool_quota_percent_remaining{tool=~"$tool"}',
    '(min by (tool, kind) (ai_usage_tool_quota_percent_remaining{tool=~"$tool"}) / (clamp_min(-deriv((min by (tool, kind) (ai_usage_tool_quota_percent_remaining{tool=~"$tool"}))[1h:]) * 3600, 0) > 0)) * 3600',
    'clamp_min(-deriv((min by (tool, kind) (ai_usage_tool_quota_percent_remaining{tool=~"$tool"}))[15m:]) * 3600, 0)',
}
missing_dashboard_queries = required_dashboard_queries - dashboard_expressions
if missing_dashboard_queries:
    fail(
        "Grafana dashboard is missing contracted queries: "
        + ", ".join(sorted(missing_dashboard_queries))
    )
usage_rate_panels = [
    panel for panel in dashboard.get("panels", [])
    if panel.get("title") == "Quota Usage Rate"
]
if len(usage_rate_panels) != 1 or usage_rate_panels[0].get("type") != "timeseries":
    fail("Grafana quota usage rate must be presented as one time-series panel")

print(f"Non-invasive monitoring OK: {len(catalog['providers'])} provider profiles")
