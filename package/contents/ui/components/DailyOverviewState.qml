import QtQuick
import org.kde.ki18n

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

    property CompactMetricState compactMetricState: CompactMetricState {
        summary: state.summary
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
        var parts = [KI18n.i18n("%1 of %2 active sources", useful, enabled)];
        var connectivity = Number(summary.connectivityOnlySourceCount || 0);
        var attention = Number(summary.attentionSourceCount || 0);
        if (connectivity > 0)
            parts.push(KI18n.i18np("%1 connectivity only", "%1 connectivity only", connectivity));
        if (attention > 0)
            parts.push(KI18n.i18np("%1 needs attention", "%1 need attention", attention));
        return parts.join(KI18n.i18n(" · "));
    }

    function headline() {
        var attention = Number(summary.attentionSourceCount || 0);
        var useful = Number(summary.reportingUsefulSourceCount || 0);
        var connectivity = Number(summary.connectivityOnlySourceCount || 0);
        if (attention > 0) return KI18n.i18n("Your daily monitor needs attention");
        if (useful > 0)
            return KI18n.i18np("%1 source is reporting", "%1 sources are reporting", useful);
        if (connectivity > 0) return KI18n.i18n("Connections work, but usage is unavailable");
        if (Number(summary.enabledSourceCount || 0) > 0)
            return KI18n.i18n("Verify a source to start daily monitoring");
        return KI18n.i18n("No monitoring source is enabled");
    }

    function explanation() {
        if (Number(summary.attentionSourceCount || 0) > 0)
            return actionText(topAction);
        if (Number(summary.reportingUsefulSourceCount || 0) > 0)
            return KI18n.i18n("Actual data, estimates, balances, and fixed fees stay separate.");
        if (Number(summary.connectivityOnlySourceCount || 0) > 0)
            return KI18n.i18n("A connection check does not prove usage, spend, or live quota.");
        return KI18n.i18n("Run guided setup to choose and verify a useful source.");
    }

    function factRows() {
        var facts = [];
        var quota = summary.lowestActualRemainingQuota || {};
        var reset = summary.nearestActualReset || {};
        if (Number(summary.attentionSourceCount || 0) > 0) {
            facts.push({
                icon: summary.highestSeverity === "critical" ? "dialog-error" : "dialog-warning",
                value: Number(summary.attentionSourceCount),
                label: KI18n.i18n("Needs attention")
            });
        }
        if (quota.stableId) {
            facts.push({
                icon: "speedometer",
                value: KI18n.i18n("%1%", Math.round(Number(quota.percentRemaining))),
                label: KI18n.i18n("Lowest quota · %1", quota.displayName)
            });
        }
        if (reset.stableId && facts.length < 3) {
            facts.push({
                icon: "chronometer",
                value: relativeReset(reset.resetAt),
                label: KI18n.i18n("Next reset · %1", reset.displayName)
            });
        }
        if (facts.length < 2 && Number(summary.reportingUsefulSourceCount || 0) > 0) {
            facts.push({
                icon: "dialog-ok",
                value: Number(summary.reportingUsefulSourceCount),
                label: KI18n.i18n("Active sources")
            });
        }
        return facts.slice(0, 3);
    }

    function actionText(row) {
        var reason = row?.attentionReasonKey || "none";
        var labels = {
            authentication: KI18n.i18n("Authentication failed. Review the credentials."),
            needs_configuration: KI18n.i18n("Complete this source's configuration."),
            unavailable_locally: KI18n.i18n("Install the local tool, then check again."),
            quota_exhausted: KI18n.i18n("The current quota is exhausted."),
            quota_critical: KI18n.i18n("The current quota is nearly exhausted."),
            budget_critical: KI18n.i18n("The configured budget has been reached."),
            stale_data: KI18n.i18n("Refresh this source to replace stale data."),
            quota_warning: KI18n.i18n("The current quota is running low."),
            budget_warning: KI18n.i18n("Spend is approaching the configured budget."),
            ready_to_verify: KI18n.i18n("Run the safe read-only verification.")
        };
        return labels[reason] || KI18n.i18n("Review this source and try again.");
    }

    function relativeReset(value, nowValue) {
        var reset = new Date(value);
        var now = nowValue ? new Date(nowValue) : new Date();
        if (!Number.isFinite(reset.getTime()) || reset.getTime() <= now.getTime())
            return KI18n.i18n("now");
        var minutes = Math.max(1, Math.ceil((reset.getTime() - now.getTime()) / 60000));
        if (minutes < 60) return KI18n.i18np("%1 min", "%1 min", minutes);
        var hours = Math.ceil(minutes / 60);
        if (hours < 48) return KI18n.i18np("%1 hour", "%1 hours", hours);
        var days = Math.ceil(hours / 24);
        return KI18n.i18np("%1 day", "%1 days", days);
    }

    function normalizeCompactMode(mode) {
        return compactMetricState.normalizeMode(mode);
    }

    function compactStatusKey() {
        return compactMetricState.statusKey();
    }

    function compactText(mode) {
        return compactMetricState.displayText(mode);
    }
}
