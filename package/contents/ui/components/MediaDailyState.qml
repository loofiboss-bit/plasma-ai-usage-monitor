import QtQuick
import com.github.loofi.aiusagemonitor 1.0

QtObject {
    id: mediaState

    readonly property string scenario: AppInfo.smokeView
    readonly property bool active: AppInfo.demoMode
        && scenario.indexOf("media-") === 0
        && ["media-overview", "media-attention", "media-quota",
            "media-tool-only", "media-panel"].indexOf(scenario) >= 0
    readonly property var rows: buildRows()
    readonly property var summary: buildSummary()

    function resetAt(hours) {
        return new Date(Date.now() + hours * 3600000).toISOString();
    }

    function baseRow(stableId, displayName, sourceKind, qualityClass) {
        return {
            stableId: stableId,
            displayName: displayName,
            sourceKind: sourceKind,
            monitoringLevel: qualityClass === "actual" ? "usage" : "estimate",
            readinessState: qualityClass === "actual"
                ? "reporting_actual" : "reporting_estimate",
            qualityClass: qualityClass,
            freshnessState: "fresh",
            attentionSeverity: "none",
            attentionReasonKey: "none",
            nextActionKey: "none",
            primaryMetricAvailable: false,
            primaryMetricValue: undefined,
            primaryMetricUnit: "",
            currency: "",
            quotaWindows: []
        };
    }

    function quotaRow(remaining, resetHours, severity) {
        var row = baseRow("codex-cli", "Codex CLI", "local_tool", "actual");
        row.primaryMetricAvailable = true;
        row.primaryMetricValue = remaining;
        row.primaryMetricUnit = "percent_remaining";
        row.attentionSeverity = severity || "none";
        row.attentionReasonKey = severity === "critical"
            ? "quota_critical" : severity === "warning" ? "quota_warning" : "none";
        row.nextActionKey = severity && severity !== "none"
            ? "sync_subscription" : "none";
        row.quotaWindows = [{
            kind: "messages",
            window: "five_hour",
            percentUsed: 100 - remaining,
            percentRemaining: remaining,
            sourceClass: "actual",
            sourceKey: "browser_sync",
            resetAt: resetAt(resetHours)
        }];
        return row;
    }

    function buildRows() {
        if (scenario === "media-attention")
            return [quotaRow(4, 2, "critical")];
        if (scenario === "media-quota" || scenario === "media-panel")
            return [quotaRow(28, 3, "none")];
        if (scenario === "media-tool-only") {
            var codex = quotaRow(46, 4, "none");
            var claude = baseRow("claude-code", "Claude Code", "local_tool",
                                 "estimated");
            claude.primaryMetricAvailable = true;
            claude.primaryMetricValue = 31;
            claude.primaryMetricUnit = "percent_remaining";
            return [codex, claude];
        }

        var openRouter = baseRow("openrouter", "OpenRouter", "provider",
                                 "actual");
        openRouter.primaryMetricAvailable = true;
        openRouter.primaryMetricValue = 0.42;
        openRouter.primaryMetricUnit = "currency";
        openRouter.currency = "USD";

        var deepSeek = baseRow("deepseek", "DeepSeek", "provider", "balance");
        deepSeek.monitoringLevel = "balance";
        deepSeek.readinessState = "reporting_actual";
        deepSeek.primaryMetricAvailable = true;
        deepSeek.primaryMetricValue = 42.75;
        deepSeek.primaryMetricUnit = "currency";
        deepSeek.currency = "USD";

        var cursor = baseRow("cursor", "Cursor", "local_tool", "estimated");
        cursor.primaryMetricAvailable = true;
        cursor.primaryMetricValue = 18;
        cursor.primaryMetricUnit = "count";

        return [openRouter, deepSeek, cursor];
    }

    function buildSummary() {
        var currentRows = rows;
        var actual = 0;
        var estimated = 0;
        var balance = 0;
        var attention = 0;
        for (var i = 0; i < currentRows.length; ++i) {
            if (currentRows[i].qualityClass === "actual") actual++;
            else if (currentRows[i].qualityClass === "estimated") estimated++;
            else if (currentRows[i].qualityClass === "balance") balance++;
            if (currentRows[i].attentionSeverity !== "none") attention++;
        }

        var quota = {};
        var reset = {};
        var codex = source("codex-cli");
        if (codex.stableId && codex.quotaWindows.length > 0) {
            var window = codex.quotaWindows[0];
            quota = {
                stableId: codex.stableId,
                displayName: codex.displayName,
                percentRemaining: window.percentRemaining,
                sourceClass: window.sourceClass
            };
            reset = {
                stableId: codex.stableId,
                displayName: codex.displayName,
                resetAt: window.resetAt,
                sourceClass: window.sourceClass
            };
        }

        var urgent = attention > 0 ? currentRows[0] : {};
        return {
            enabledSourceCount: currentRows.length,
            reportingUsefulSourceCount: currentRows.length,
            actualSourceCount: actual,
            estimatedSourceCount: estimated,
            balanceSourceCount: balance,
            connectivityOnlySourceCount: 0,
            attentionSourceCount: attention,
            staleSourceCount: 0,
            highestSeverity: attention > 0 ? "critical" : "none",
            mostUrgentSource: urgent,
            lowestRemainingQuota: quota,
            nearestReset: reset,
            lowestActualRemainingQuota: quota,
            nearestActualReset: reset,
            actualSpendTotals: scenario === "media-overview" ? { USD: 0.42 } : {},
            estimatedSpendTotals: scenario === "media-overview" ? { USD: 0.18 } : {},
            fixedSubscriptionFees: scenario === "media-overview" ? { USD: 20 } : {},
            providerActualSpendTotals: scenario === "media-overview" ? { USD: 6.25 } : {},
            providerDailyActualSpendTotals: scenario === "media-overview" ? { USD: 0.42 } : {},
            remainingRequests: {}
        };
    }

    function prioritizedSourceIds() {
        return rows.map(function(row) { return row.stableId; });
    }

    function source(stableId) {
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].stableId === stableId) return rows[i];
        }
        return {};
    }
}
