pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.Frame {
    id: comparison
    objectName: "sessionComparison"
    implicitWidth: Kirigami.Units.gridUnit * 20
    Layout.minimumWidth: 0
    required property var sessionModel
    required property string toolId
    property string fromUtc: ""
    property string toUtc: ""
    property string projectId: ""
    property var modelFilter: ({})
    property int refreshRevision: 0
    property int breakdownIndex: 0
    property var projectLabel: function() { return i18n("Unnamed project"); }
    signal scopeRequested(string projectId, var modelFilter)
    signal toolScopeRequested(string toolId)
    property bool active: false
    property bool loading: false
    property string errorKey: ""
    property string requestId: ""
    property int generation: 0
    readonly property string requestPrefix: Date.now().toString(36)
        + "-" + Math.random().toString(36).slice(2)
    property var result: ({})
    readonly property var metricRows: [
        {key: "sessionCount", label: i18n("Sessions with activity")},
        {key: "totalTokens", label: i18n("Total tokens")},
        {key: "inputTokens", label: i18n("Input tokens")},
        {key: "outputTokens", label: i18n("Output tokens")},
        {key: "cacheReadTokens", label: i18n("Cache read tokens (part of input)")},
        {key: "cacheWriteTokens", label: i18n("Cache write tokens (part of input)")}
    ]

    function numberText(value) {
        return typeof value === "number" && Number.isFinite(value)
            ? value.toLocaleString(Qt.locale(), "f", 0) : i18n("Unavailable");
    }

    function reasonText(reason) {
        if (reason === "zero-baseline") return i18n("Previous value is zero; percentage unavailable");
        if (reason === "no-observations") return i18n("No recorded observations in one or both periods");
        if (reason === "missing-category") return i18n("Some observations omit this token category");
        return i18n("Comparison unavailable because coverage is incomplete");
    }

    function deltaText(delta) {
        if (!delta || !delta.available) return reasonText(delta ? delta.reason : "");
        var text = (delta.absolute > 0 ? "+" : "") + numberText(delta.absolute);
        if (delta.percentAvailable)
            return text + " · " + (delta.percent > 0 ? "+" : "")
                + Number(delta.percent).toLocaleString(Qt.locale(), "f", 1) + "%";
        return text + " · " + reasonText(delta.reason);
    }

    function periodText(period) {
        if (!period || !period.fromUtc || !period.toUtc) return "";
        var from = new Date(period.fromUtc);
        var through = new Date(new Date(period.toUtc).getTime() - 1);
        return i18n("%1 – %2", from.toLocaleDateString(Qt.locale(), Locale.ShortFormat),
                    through.toLocaleDateString(Qt.locale(), Locale.ShortFormat));
    }

    function categoryText(period, key) {
        var category = period && period.categoryCoverage ? period.categoryCoverage[key] : null;
        if (!category) return "";
        return i18n("%1 reported · %2 missing", numberText(category.reportedEventCount),
                    numberText(category.missingEventCount));
    }

    function toolDisplayName(id) {
        if (id === "codex-cli" || id === "codex") return i18n("Codex");
        if (id === "claude-code") return i18n("Claude Code");
        return id || i18n("Unknown tool");
    }

    function rowMetricsSuffix(row) {
        if (!row) return "";
        var parts = [];
        if (typeof row.sharePercent === "number" && Number.isFinite(row.sharePercent))
            parts.push(i18n("%1% share", Number(row.sharePercent).toLocaleString(Qt.locale(), "f", 1)));
        if (typeof row.cacheHitPercent === "number" && Number.isFinite(row.cacheHitPercent))
            parts.push(i18n("%1% cache read", Number(row.cacheHitPercent).toLocaleString(Qt.locale(), "f", 1)));
        return parts.length ? " · " + parts.join(" · ") : "";
    }

    function insightText(insight) {
        if (insight.key === "top-tool")
            return i18n("Tool contribution in recorded usage: %1 · %2 tokens",
                        toolDisplayName(insight.toolId), numberText(insight.value));
        if (insight.key === "top-model")
            return i18n("Model contribution in recorded usage: %1 · %2 tokens",
                        insight.modelId || i18n("Unknown model"), numberText(insight.value));
        if (insight.key === "top-project")
            return i18n("Project contribution in recorded usage: %1 · %2 tokens",
                        projectLabel(insight.projectId), numberText(insight.value));
        if (insight.key === "input-share" || insight.key === "output-share" || insight.key === "cache-read-share") {
            var label = insight.key === "input-share" ? i18n("Input share of recorded tokens")
                : insight.key === "output-share" ? i18n("Output share of recorded tokens")
                : i18n("Cache read share of recorded input");
            return i18n("%1: %2%", label, Number(insight.value).toLocaleString(Qt.locale(), "f", 1));
        }
        return "";
    }

    function refresh() {
        requestId = "";
        result = ({});
        errorKey = "";
        loading = false;
        if (!active || !sessionModel || !fromUtc || !toUtc) return;
        if (typeof sessionModel.requestComparison !== "function") {
            errorKey = "session-comparison-unavailable";
            return;
        }
        requestId = "session-comparison-" + requestPrefix + "-" + (++generation);
        loading = true;
        sessionModel.requestComparison(requestId, toolId, fromUtc, toUtc,
                                       projectId, "", modelFilter);
    }

    function scheduleRefresh() { Qt.callLater(comparison.refresh); }
    onActiveChanged: scheduleRefresh()
    onRefreshRevisionChanged: scheduleRefresh()
    onSessionModelChanged: scheduleRefresh()
    onToolIdChanged: scheduleRefresh()
    onFromUtcChanged: scheduleRefresh()
    onToUtcChanged: scheduleRefresh()
    onProjectIdChanged: scheduleRefresh()
    onModelFilterChanged: scheduleRefresh()
    Component.onCompleted: scheduleRefresh()

    Connections {
        target: comparison.sessionModel
        ignoreUnknownSignals: true
        function onComparisonReady(id, payload) {
            if (!comparison.active || id !== comparison.requestId) return;
            comparison.result = payload || ({});
            comparison.loading = false;
        }
        function onQueryFailed(id, key) {
            if (id !== comparison.requestId) return;
            comparison.errorKey = key || "session-query-failed";
            comparison.loading = false;
        }
        function onClearFinished() { comparison.refresh(); }
        function onEnabledChanged() { comparison.refresh(); }
    }

    contentItem: ColumnLayout {
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 4
            text: i18n("Previous period comparison")
            wrapMode: Text.Wrap
        }
        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Reported local usage only. Missing logs are not zero usage. Cache tokens are already included in input.")
            wrapMode: Text.Wrap
        }
        QQC2.BusyIndicator { running: comparison.loading; visible: running }
        QQC2.Label {
            Layout.fillWidth: true
            visible: comparison.errorKey !== ""
            text: i18n("The comparison could not be loaded. Try again.")
            wrapMode: Text.Wrap
            color: Kirigami.Theme.negativeTextColor
        }
        QQC2.Button {
            visible: comparison.errorKey !== ""
            text: i18n("Retry comparison")
            onClicked: comparison.refresh()
        }
        QQC2.Label {
            objectName: "sessionComparisonPeriods"
            Layout.fillWidth: true
            visible: !comparison.loading && !!comparison.result.current
            text: i18n("Current: %1\nPrevious: %2", comparison.periodText(comparison.result.current),
                        comparison.periodText(comparison.result.previous))
            wrapMode: Text.Wrap
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: !!comparison.result.coverageStartUtc
            text: comparison.result.coverageStartUtc
                ? i18n("Comparable recorded history starts %1. Earlier periods may be incomplete.",
                    new Date(comparison.result.coverageStartUtc).toLocaleDateString(Qt.locale(), Locale.ShortFormat))
                : ""
            wrapMode: Text.Wrap
            color: Kirigami.Theme.disabledTextColor
        }
        Repeater {
            model: comparison.result.current ? comparison.metricRows : []
            delegate: QQC2.Frame {
                id: metric
                required property var modelData
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    QQC2.Label { Layout.fillWidth: true; text: metric.modelData.label; font.bold: true; wrapMode: Text.Wrap }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: i18n("%1 → %2", comparison.numberText((comparison.result.previous || {})[metric.modelData.key]),
                                    comparison.numberText((comparison.result.current || {})[metric.modelData.key]))
                        wrapMode: Text.Wrap
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: comparison.deltaText((comparison.result.deltas || {})[metric.modelData.key])
                        wrapMode: Text.Wrap
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: metric.modelData.key !== "sessionCount"
                        text: i18n("Current: %1\nPrevious: %2",
                            comparison.categoryText(comparison.result.current, metric.modelData.key),
                            comparison.categoryText(comparison.result.previous, metric.modelData.key))
                        wrapMode: Text.Wrap
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
        Repeater {
            model: comparison.result.insights || []
            delegate: QQC2.Label {
                required property var modelData
                Layout.fillWidth: true
                text: comparison.insightText(modelData)
                visible: text !== ""
                wrapMode: Text.Wrap
            }
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: comparison.breakdownIndex !== 0
            text: comparison.breakdownIndex === 1 ? i18n("Model comparisons")
                : comparison.breakdownIndex === 2 ? i18n("Project comparisons")
                : i18n("Tool comparisons")
            font.bold: true
        }
        ListView {
            id: breakdownList
            objectName: "sessionComparisonBreakdown"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            implicitHeight: Math.min(count, 5) * Kirigami.Units.gridUnit * 5
            visible: comparison.breakdownIndex !== 0 && count > 0
            clip: true
            reuseItems: true
            model: comparison.breakdownIndex === 1 ? (comparison.result.modelBreakdown || [])
                : comparison.breakdownIndex === 2 ? (comparison.result.projectBreakdown || [])
                : comparison.breakdownIndex === 3 ? (comparison.result.toolBreakdown || []) : []
            QQC2.ScrollBar.vertical: QQC2.ScrollBar { }
            delegate: QQC2.ItemDelegate {
                id: comparisonRow
                required property var modelData
                width: breakdownList.width
                contentItem: QQC2.Label {
                    id: comparisonRowLabel
                    text: i18n("%1 · %2 → %3 · %4",
                        comparison.breakdownIndex === 1
                            ? (comparisonRow.modelData.modelId || i18n("Unknown model"))
                            : comparison.breakdownIndex === 2
                              ? comparison.projectLabel(comparisonRow.modelData.projectId)
                              : comparison.toolDisplayName(comparisonRow.modelData.toolId),
                        comparison.numberText((comparisonRow.modelData.previous || {}).totalTokens),
                        comparison.numberText((comparisonRow.modelData.current || {}).totalTokens),
                        comparison.deltaText((comparisonRow.modelData.deltas || {}).totalTokens))
                        + comparison.rowMetricsSuffix(comparisonRow.modelData)
                    wrapMode: Text.Wrap
                }
                Accessible.name: comparisonRowLabel.text
                onClicked: {
                    if (comparison.breakdownIndex === 3 && modelData.toolId) {
                        comparison.toolScopeRequested(modelData.toolId);
                    } else {
                        comparison.scopeRequested(
                            comparison.breakdownIndex === 2 ? modelData.projectId : comparison.projectId,
                            comparison.breakdownIndex === 1 ? {modelId: modelData.modelId} : comparison.modelFilter);
                    }
                }
            }
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: comparison.breakdownIndex !== 0
            text: i18n("Showing %1 of %2", breakdownList.count,
                comparison.breakdownIndex === 1
                    ? (comparison.result.modelBreakdownTotalCount || breakdownList.count)
                    : comparison.breakdownIndex === 2
                      ? (comparison.result.projectBreakdownTotalCount || breakdownList.count)
                      : (comparison.result.toolBreakdownTotalCount || breakdownList.count))
            wrapMode: Text.Wrap
        }
    }
}
