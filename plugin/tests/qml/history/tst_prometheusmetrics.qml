import QtQuick
import QtTest
import "../../../../package/contents/ui/PrometheusMetrics.js" as PrometheusMetrics

TestCase {
    name: "PrometheusMetrics"
    readonly property double nowMs: Date.parse("2026-08-23T10:00:00Z")

    function test_actualQuotaWindow() {
        var lines = [];
        PrometheusMetrics.appendToolQuotaMetrics(lines, "codex_cli", [{
            kind: "rolling_5h",
            percentRemaining: 96,
            observedAt: "2026-08-23T09:55:00Z",
            resetAt: "2026-08-23T12:00:00Z",
            source: "browser_sync",
            precision: "browser_sync_actual"
        }], nowMs);

        compare(lines.length, 2);
        compare(lines[0], "ai_usage_tool_quota_percent_remaining{tool=\"codex_cli\",kind=\"rolling_5h\",source=\"browser_sync\",quality=\"browser_sync_actual\"} 96");
        compare(lines[1], "ai_usage_tool_quota_reset_timestamp_seconds{tool=\"codex_cli\",kind=\"rolling_5h\",source=\"browser_sync\",quality=\"browser_sync_actual\"} 1787486400");
    }

    function test_nonActualAndInvalidQuotaWindows() {
        var lines = [];
        PrometheusMetrics.appendToolQuotaMetrics(lines, "codex_cli", [
            { kind: "local", percentRemaining: 50, source: "self_tracked" },
            { kind: "broken", percentRemaining: "not-a-number", source: "browser_sync" }
        ], nowMs);

        compare(lines.length, 0);
    }

    function test_missingResetAndEscapedLabels() {
        var lines = [];
        PrometheusMetrics.appendToolQuotaMetrics(lines, "tool\\name", [{
            kind: "quoted\"window",
            percentRemaining: 25,
            observedAt: "2026-08-23T09:55:00Z",
            source: "antigravity_local",
            precision: "local_daemon_actual"
        }], nowMs);

        compare(lines.length, 1);
        compare(lines[0], "ai_usage_tool_quota_percent_remaining{tool=\"tool\\\\name\",kind=\"quoted\\\"window\",source=\"antigravity_local\",quality=\"local_daemon_actual\"} 25");
    }

    function test_labelLineBreaksAreEscaped() {
        compare(PrometheusMetrics.labelValue("first\r\nsecond\rthird\nfourth"),
                "first\\nsecond\\nthird\\nfourth");
    }

    function test_percentUsedOnlyWindowExportsRemainingQuota() {
        var lines = [];
        PrometheusMetrics.appendToolQuotaMetrics(lines, "claude_code", [{
            kind: "five_hour",
            percentUsed: 37,
            observedAt: "2026-08-23T09:55:00Z",
            resetAt: "2026-08-23T12:00:00Z",
            source: "browser_sync",
            precision: "browser_sync_actual"
        }], nowMs);

        compare(lines[0], "ai_usage_tool_quota_percent_remaining{tool=\"claude_code\",kind=\"five_hour\",source=\"browser_sync\",quality=\"browser_sync_actual\"} 63");
    }

    function test_staleAndExpiredWindowsAreNotExported() {
        var lines = [];
        PrometheusMetrics.appendToolQuotaMetrics(lines, "codex_cli", [
            {
                kind: "stale",
                percentRemaining: 40,
                observedAt: "2026-08-23T09:45:00Z",
                resetAt: "2026-08-23T12:00:00Z",
                source: "usage_api"
            },
            {
                kind: "expired",
                percentRemaining: 20,
                observedAt: "2026-08-23T09:55:00Z",
                resetAt: "2026-08-23T10:00:00Z",
                source: "usage_api"
            }
        ], nowMs);

        compare(lines.length, 0);
    }

    function test_moneyRejectsUnknownCurrencyAndMissingValuesButKeepsZero() {
        var totals = {};
        verify(!PrometheusMetrics.addCurrencyValue(totals, "", 5));
        verify(!PrometheusMetrics.addCurrencyValue(totals, "ZZZ", 5));
        verify(!PrometheusMetrics.addCurrencyValue(totals, "USD", undefined));
        verify(!PrometheusMetrics.addCurrencyValue(totals, "USD", "0"));
        verify(PrometheusMetrics.addCurrencyValue(totals, "USD", 0));
        compare(totals.USD, 0);

        var lines = [];
        PrometheusMetrics.appendCurrencyMetrics(lines, "ai_usage_api_spend",
                                                "period=\"month\"",
                                                { USD: 0, ZZZ: 12 });
        compare(lines.length, 1);
        compare(lines[0], "ai_usage_api_spend{period=\"month\",currency=\"USD\"} 0");
    }

    function test_costClassesStaySeparateWithoutMonthlyExposureTotal() {
        var lines = [];
        PrometheusMetrics.appendCostClassMetrics(lines, {
            actualCurrent: { USD: 1.25 },
            actualToday: { USD: 3.5 },
            actualMonth: { EUR: 9 },
            subscriptionFees: { USD: 20 },
            estimatedBurn: { USD: 4.5 }
        });

        compare(lines.join("\n"), [
            "ai_usage_api_spend{period=\"current\",currency=\"USD\"} 1.25",
            "ai_usage_api_spend{period=\"today\",currency=\"USD\"} 3.5",
            "ai_usage_api_spend{period=\"month\",currency=\"EUR\"} 9",
            "ai_usage_subscription_fees{period=\"month\",cost_source=\"self_tracked\",currency=\"USD\"} 20",
            "ai_usage_estimated_burn{period=\"month\",cost_source=\"estimated_from_usage\",currency=\"USD\"} 4.5"
        ].join("\n"));
        verify(lines.every(function(line) {
            return line.indexOf("ai_usage_total_monthly_exposure") < 0;
        }));
    }

    function test_staleMetricIsNotAvailableForCurrentExport() {
        var now = Date.parse("2026-08-23T10:00:00Z");
        verify(PrometheusMetrics.metricIsFresh({
            available: true, value: 0, observedAt: "2026-08-23T09:59:00Z"
        }, now));
        verify(!PrometheusMetrics.metricIsFresh({
            available: true, value: 10, observedAt: "2026-08-23T09:44:59Z"
        }, now));
        verify(!PrometheusMetrics.metricIsFresh({
            available: true, value: 10
        }, now));
    }

    function test_localActivityExportRequiresFreshObservationAndPreservesZero() {
        var now = Date.parse("2026-08-23T10:00:00Z");
        var noObservation = [];
        PrometheusMetrics.appendLocalActivityMetrics(
            noObservation, "codex_cli", 4, 100, undefined, now);
        compare(noObservation.join("\n"),
                "ai_usage_tool_usage_limit{tool=\"codex_cli\"} 100");

        var staleObservation = [];
        PrometheusMetrics.appendLocalActivityMetrics(
            staleObservation, "codex_cli", 4, 100,
            "2026-08-23T09:44:59Z", now);
        compare(staleObservation.length, 2);
        verify(staleObservation[0].indexOf("usage_limit") >= 0);
        verify(staleObservation[1].indexOf("last_activity_seconds") >= 0);

        var observedZero = [];
        PrometheusMetrics.appendLocalActivityMetrics(
            observedZero, "codex_cli", 0, 100,
            "2026-08-23T09:59:00Z", now);
        compare(observedZero.length, 4);
        verify(observedZero[0].endsWith(" 0"));
        verify(observedZero[1].endsWith(" 0"));
        verify(observedZero[2].indexOf("usage_limit") >= 0);
        verify(observedZero[3].indexOf("last_activity_seconds") >= 0);
    }
}
