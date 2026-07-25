import QtQuick
import org.kde.ki18n
import "../Utils.js" as Utils

QtObject {
    id: state

    property var summary: ({})

    property MetricAvailabilityFormatter formatter: MetricAvailabilityFormatter {}

    function normalizeMode(mode) {
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

    function statusKey() {
        var severity = summary.highestSeverity || "none";
        if (severity !== "none") return severity;
        if (Number(summary.reportingUsefulSourceCount || 0) > 0) return "healthy";
        if (Number(summary.connectivityOnlySourceCount || 0) > 0) return "connectivity";
        return Number(summary.enabledSourceCount || 0) > 0 ? "unverified" : "hidden";
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

    function displayText(mode) {
        var normalized = normalizeMode(mode);
        if (normalized === "attention") {
            var urgent = summary.mostUrgentSource || {};
            if (urgent.stableId) return urgent.displayName;
            if (Number(summary.reportingUsefulSourceCount || 0) > 0) return KI18n.i18n("All clear");
            if (Number(summary.connectivityOnlySourceCount || 0) > 0)
                return KI18n.i18n("Connectivity only");
            return formatter.unavailableValue();
        }
        if (normalized === "lowest-quota") {
            var quota = summary.lowestActualRemainingQuota || {};
            return quota.stableId
                ? KI18n.i18n("%1% · %2", Math.round(Number(quota.percentRemaining)), quota.displayName)
                : formatter.unavailableValue();
        }
        if (normalized === "next-reset") {
            var reset = summary.nearestActualReset || {};
            return reset.stableId
                ? relativeReset(reset.resetAt) : formatter.unavailableValue();
        }
        if (normalized === "actual-spend")
            return Utils.formatCurrencyTotals(summary.providerActualSpendTotals || {});
        if (normalized === "active-sources")
            return Number(summary.reportingUsefulSourceCount || 0).toString();
        if (normalized === "dailycost")
            return Utils.formatCurrencyTotals(summary.providerDailyActualSpendTotals || {});
        if (normalized === "requests") {
            var requests = summary.remainingRequests || {};
            return requests.stableId
                ? KI18n.i18n("%1 req", requests.value) : formatter.unavailableValue();
        }
        return "";
    }

    function summaryText() {
        var enabled = Number(summary.enabledSourceCount || 0);
        var useful = Number(summary.reportingUsefulSourceCount || 0);
        var parts = [];
        var urgent = summary.mostUrgentSource || {};
        var severity = summary.highestSeverity || "none";
        if (urgent.stableId && (severity === "critical" || severity === "warning")) {
            parts.push(severity === "critical"
                ? KI18n.i18n("Critical: %1", urgent.displayName)
                : KI18n.i18n("Warning: %1", urgent.displayName));
        }
        parts.push(KI18n.i18n("%1 of %2 active sources", useful, enabled));
        var connectivity = Number(summary.connectivityOnlySourceCount || 0);
        var attention = Number(summary.attentionSourceCount || 0);
        if (connectivity > 0)
            parts.push(KI18n.i18np("%1 connectivity only", "%1 connectivity only", connectivity));
        if (attention > 0)
            parts.push(KI18n.i18np("%1 needs attention", "%1 need attention", attention));
        return parts.join(KI18n.i18n(" · "));
    }
}
