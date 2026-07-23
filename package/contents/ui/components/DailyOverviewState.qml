import QtQuick
import "../Utils.js" as Utils

QtObject {
    id: state

    property var dailyState: null
    property int revision: 0

    readonly property var summary: dailyState && dailyState.summary
        ? dailyState.summary : emptySummary()
    readonly property var sourceRows: buildSourceRows()
    readonly property var attentionRows: filterRows(function(row) {
        return row.attentionSeverity && row.attentionSeverity !== "none";
    })
    readonly property var actualRows: qualityRows("actual")
    readonly property var estimatedRows: qualityRows("estimated")
    readonly property var balanceRows: qualityRows("balance")
    readonly property var connectivityRows: qualityRows("connectivity_only")
    readonly property var unavailableRows: qualityRows("unavailable")
    readonly property var topAction: attentionRows.length > 0 ? attentionRows[0] : ({})

    property Connections dailyStateConnections: Connections {
        target: state.dailyState
        enabled: state.dailyState !== null
        ignoreUnknownSignals: true

        function onSummaryChanged() { state.revision++; }
        function onSourceChanged(stableId) { state.revision++; }
        function onCountChanged() { state.revision++; }
    }

    function emptySummary() {
        return {
            enabledSourceCount: 0,
            reportingUsefulSourceCount: 0,
            actualSourceCount: 0,
            estimatedSourceCount: 0,
            balanceSourceCount: 0,
            connectivityOnlySourceCount: 0,
            attentionSourceCount: 0,
            staleSourceCount: 0,
            highestSeverity: "none",
            mostUrgentSource: ({}),
            lowestRemainingQuota: ({}),
            nearestReset: ({}),
            lowestActualRemainingQuota: ({}),
            nearestActualReset: ({}),
            actualSpendTotals: ({}),
            estimatedSpendTotals: ({}),
            fixedSubscriptionFees: ({}),
            providerActualSpendTotals: ({}),
            providerDailyActualSpendTotals: ({}),
            remainingRequests: ({})
        };
    }

    function buildSourceRows() {
        var currentRevision = revision;
        if (!dailyState || typeof dailyState.prioritizedSourceIds !== "function"
                || typeof dailyState.source !== "function") return [];
        var ids = dailyState.prioritizedSourceIds() || [];
        var rows = [];
        for (var i = 0; i < ids.length; i++) {
            var row = dailyState.source(ids[i]);
            if (row && row.stableId) rows.push(row);
        }
        return rows;
    }

    function filterRows(predicate) {
        var result = [];
        for (var i = 0; i < sourceRows.length; i++) {
            if (predicate(sourceRows[i])) result.push(sourceRows[i]);
        }
        return result;
    }

    function qualityRows(quality) {
        return filterRows(function(row) { return row.qualityClass === quality; });
    }

    function summaryText() {
        var enabled = Number(summary.enabledSourceCount || 0);
        var useful = Number(summary.reportingUsefulSourceCount || 0);
        var parts = [i18n("%1 of %2 active sources", useful, enabled)];
        var connectivity = Number(summary.connectivityOnlySourceCount || 0);
        var attention = Number(summary.attentionSourceCount || 0);
        if (connectivity > 0)
            parts.push(i18np("%1 connectivity only", "%1 connectivity only", connectivity));
        if (attention > 0)
            parts.push(i18np("%1 needs attention", "%1 need attention", attention));
        return parts.join(i18n(" · "));
    }

    function headline() {
        var attention = Number(summary.attentionSourceCount || 0);
        var useful = Number(summary.reportingUsefulSourceCount || 0);
        var connectivity = Number(summary.connectivityOnlySourceCount || 0);
        if (attention > 0) return i18n("Your daily monitor needs attention");
        if (useful > 0)
            return i18np("%1 source is reporting", "%1 sources are reporting", useful);
        if (connectivity > 0) return i18n("Connections work, but usage is unavailable");
        if (Number(summary.enabledSourceCount || 0) > 0)
            return i18n("Verify a source to start daily monitoring");
        return i18n("No monitoring source is enabled");
    }

    function explanation() {
        if (Number(summary.attentionSourceCount || 0) > 0)
            return actionText(topAction);
        if (Number(summary.reportingUsefulSourceCount || 0) > 0)
            return i18n("Actual data, estimates, balances, and fixed fees stay separate.");
        if (Number(summary.connectivityOnlySourceCount || 0) > 0)
            return i18n("A connection check does not prove usage, spend, or live quota.");
        return i18n("Run guided setup to choose and verify a useful source.");
    }

    function factRows() {
        var facts = [];
        var quota = summary.lowestActualRemainingQuota || {};
        var reset = summary.nearestActualReset || {};
        if (Number(summary.attentionSourceCount || 0) > 0) {
            facts.push({
                icon: summary.highestSeverity === "critical" ? "dialog-error" : "dialog-warning",
                value: Number(summary.attentionSourceCount),
                label: i18n("Needs attention")
            });
        }
        if (quota.stableId) {
            facts.push({
                icon: "speedometer",
                value: i18n("%1%", Math.round(Number(quota.percentRemaining))),
                label: i18n("Lowest quota · %1", quota.displayName)
            });
        }
        if (reset.stableId && facts.length < 3) {
            facts.push({
                icon: "chronometer",
                value: relativeReset(reset.resetAt),
                label: i18n("Next reset · %1", reset.displayName)
            });
        }
        if (facts.length < 2 && Number(summary.reportingUsefulSourceCount || 0) > 0) {
            facts.push({
                icon: "dialog-ok",
                value: Number(summary.reportingUsefulSourceCount),
                label: i18n("Active sources")
            });
        }
        return facts.slice(0, 3);
    }

    function actionText(row) {
        var reason = row?.attentionReasonKey || "none";
        var labels = {
            authentication: i18n("Authentication failed. Review the credentials."),
            needs_configuration: i18n("Complete this source's configuration."),
            unavailable_locally: i18n("Install the local tool, then check again."),
            quota_exhausted: i18n("The current quota is exhausted."),
            quota_critical: i18n("The current quota is nearly exhausted."),
            budget_critical: i18n("The configured budget has been reached."),
            stale_data: i18n("Refresh this source to replace stale data."),
            quota_warning: i18n("The current quota is running low."),
            budget_warning: i18n("Spend is approaching the configured budget."),
            ready_to_verify: i18n("Run the safe read-only verification.")
        };
        return labels[reason] || i18n("Review this source and try again.");
    }

    function relativeReset(value, nowValue) {
        var reset = new Date(value);
        var now = nowValue ? new Date(nowValue) : new Date();
        if (!Number.isFinite(reset.getTime()) || reset.getTime() <= now.getTime())
            return i18n("now");
        var minutes = Math.max(1, Math.ceil((reset.getTime() - now.getTime()) / 60000));
        if (minutes < 60) return i18np("%1 min", "%1 min", minutes);
        var hours = Math.ceil(minutes / 60);
        if (hours < 48) return i18np("%1 hour", "%1 hours", hours);
        var days = Math.ceil(hours / 24);
        return i18np("%1 day", "%1 days", days);
    }

    function normalizeCompactMode(mode) {
        var migration = {
            count: "active-sources",
            critical: "attention",
            cost: "actual-spend"
        };
        var normalized = migration[mode] || mode;
        var supported = ["icon", "attention", "lowest-quota", "next-reset",
                         "actual-spend", "active-sources", "dailycost", "requests"];
        return supported.indexOf(normalized) >= 0 ? normalized : "icon";
    }

    function compactStatusKey() {
        var severity = summary.highestSeverity || "none";
        if (severity !== "none") return severity;
        if (Number(summary.reportingUsefulSourceCount || 0) > 0) return "healthy";
        if (Number(summary.connectivityOnlySourceCount || 0) > 0) return "connectivity";
        return Number(summary.enabledSourceCount || 0) > 0 ? "unverified" : "hidden";
    }

    function compactText(mode) {
        var normalized = normalizeCompactMode(mode);
        if (normalized === "attention") {
            var urgent = summary.mostUrgentSource || {};
            if (urgent.stableId) return urgent.displayName;
            if (Number(summary.reportingUsefulSourceCount || 0) > 0) return i18n("All clear");
            if (Number(summary.connectivityOnlySourceCount || 0) > 0)
                return i18n("Connectivity only");
            return "\u2014";
        }
        if (normalized === "lowest-quota") {
            var quota = summary.lowestActualRemainingQuota || {};
            return quota.stableId
                ? i18n("%1% · %2", Math.round(Number(quota.percentRemaining)), quota.displayName)
                : "\u2014";
        }
        if (normalized === "next-reset") {
            var reset = summary.nearestActualReset || {};
            return reset.stableId ? relativeReset(reset.resetAt) : "\u2014";
        }
        if (normalized === "actual-spend")
            return Utils.formatCurrencyTotals(summary.providerActualSpendTotals || {});
        if (normalized === "active-sources")
            return Number(summary.reportingUsefulSourceCount || 0).toString();
        if (normalized === "dailycost")
            return Utils.formatCurrencyTotals(summary.providerDailyActualSpendTotals || {});
        if (normalized === "requests") {
            var requests = summary.remainingRequests || {};
            return requests.stableId ? i18n("%1 req", requests.value) : "\u2014";
        }
        return "";
    }
}
