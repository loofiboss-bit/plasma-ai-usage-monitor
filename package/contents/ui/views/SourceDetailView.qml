pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import ".." as Monitor
import "../components" as Components
import "../SourceHealth.js" as SourceHealth
import "../Utils.js" as Utils

QQC2.ScrollView {
    id: detail
    contentWidth: availableWidth
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    property var monitor: null
    property string sourceId: ""
    property bool showAllScopes: false
    property bool technicalDetailsExpanded: false
    property bool analysisExpanded: false
    property bool dataQualityExpanded: false
    property var sessionState: null
    property double healthClockMs: Date.now()
    readonly property bool mediaMode: AppInfo.demoMode
        && ["media-source-detail", "media-coding-sessions"]
            .indexOf(AppInfo.smokeView) >= 0
    readonly property bool forecastsEnabled: !!(Plasmoid.configuration
        && Plasmoid.configuration.forecastUiEnabled)
    readonly property bool hasSessionSummary: mediaMode
        ? AppInfo.smokeView === "media-coding-sessions"
        : sourceId === "codex-cli" || sourceId === "claude-code"
    readonly property bool retryBlocked: SourceHealth.retryBlocked(
        sourceData.retryAfter, healthClockMs)
    readonly property var mediaSource: mediaMode && monitor
        ? monitor.presentationDailyState.source(sourceId) : ({})
    readonly property var sourceData: mediaMode ? mediaSource : detailModel.source
    readonly property var quotaData: mediaMode
        ? (mediaSource.quotaWindows || []) : detailModel.quotaWindows
    readonly property var coverageData: mediaMode
        ? ({ availableMetricCount: 2, totalMetricCount: 3 })
        : detailModel.coverage
    readonly property string actionLabelData: mediaMode
        ? mediaSource.nextActionKey === "none"
            ? "" : i18n("Open source settings")
        : detailModel.actionLabel
    readonly property var mediaMetrics: mediaMode
        ? (mediaSource.detailMetrics || []) : []
    readonly property var catalogModelData: catalogModel()
    readonly property var priceChangeData: catalogModelData.priceChange || ({})
    readonly property var costLabData: costLabRows()
    readonly property var runwayData: runwayRows()
    readonly property var warningRunwayData: runwayData.filter(function(row) {
        return ["warning", "critical", "exceeded"].indexOf(row.state) >= 0;
    })
    readonly property var analysisRunwayData: runwayData.filter(function(row) {
        return ["warning", "critical", "exceeded"].indexOf(row.state) < 0;
    })
    readonly property var scopeData: mediaMode
        ? [] : scopeRows()
    readonly property int totalScopeRows: mediaMode
        ? 0 : (detailModel.scopeBreakdown.scopedRows || []).length
    signal backRequested()
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    signal settingsRequested(string stableId)
    signal historyRequested(string historyId, string metric, int rangeDays)
    signal sessionHistoryRequested(string toolId, string projectId, int rangeDays,
                                   string sessionId, int pageOffset, var modelFilter)
    Accessible.role: Accessible.Pane
    Accessible.name: sourceData.displayName
        ? i18n("%1 source details", sourceData.displayName)
        : i18n("Source details")

    SourceDetailModel {
        id: detailModel
        sourceId: detail.sourceId
    }

    function statusText() {
        var labels = {
            actual: i18n("Provider-reported data"),
            estimated: i18n("Local estimate"),
            balance: i18n("Account balance"),
            connectivity_only: i18n("Connectivity only"),
            unavailable: i18n("Data unavailable")
        };
        var freshnessLabels = {
            fresh: i18n("checked recently"),
            aging: i18n("getting old"),
            stale: i18n("stale"),
            never: i18n("not observed yet")
        };
        var text = (labels[sourceData.qualityClass]
            || i18n("Data unavailable"))
            + i18n(" · %1", freshnessLabels[sourceData.freshnessState]
                               || i18n("freshness unknown"));
        var avail = Number(detail.coverageData.availableMetricCount || 0);
        var total = Number(detail.coverageData.totalMetricCount || 0);
        if (total > 0 && avail < total) {
            text += i18n(" · %1 of %2 metrics reported", avail, total);
        }
        return text;
    }

    function dateMilliseconds(value) {
        return SourceHealth.dateMilliseconds(value);
    }

    function localizedDate(value) {
        var milliseconds = dateMilliseconds(value);
        return milliseconds > 0
            ? Utils.formatWhen(milliseconds, "dateTime")
            : i18n("Not available");
    }

    function shortDate(value) {
        var milliseconds = dateMilliseconds(value);
        return milliseconds > 0
            ? Utils.formatWhen(milliseconds, "relative")
            : i18n("Not available");
    }

    function quotaRemaining(row) {
        if (!row || row.available === false) return null;
        var value = row.percentRemaining;
        if (value === undefined || value === null || value === "") return null;
        var number = Number(value);
        return Number.isFinite(number) && number >= 0 && number <= 100
            ? number : null;
    }

    function quotaLabel(row) {
        if (row.label) return row.label;
        var window = row.window || row.kind || "";
        var labels = {
            five_hour: i18n("5-hour limit"),
            "five-hour": i18n("5-hour limit"),
            weekly: i18n("Weekly limit"),
            week: i18n("Weekly limit"),
            daily: i18n("Daily limit"),
            day: i18n("Daily limit"),
            monthly: i18n("Monthly limit"),
            month: i18n("Monthly limit")
        };
        return labels[window] || window || i18n("Quota");
    }

    function summaryMetric(value, unit, currency) {
        if (value === undefined || value === null || value === ""
                || !Number.isFinite(Number(value))) return i18n("Unavailable");
        if (currency || unit === "percent_remaining")
            return formatMetric(value, unit, currency);
        return i18n("%1 %2", Utils.formatNumber(Number(value)), unit || "");
    }

    function expectedCapabilityText() {
        if (sourceData.sourceKind === "local_tool") {
            if (sourceData.readinessState === "waiting_for_activity")
                return i18n("Expected data: local activity appears after the detected app is used. Usage remains unavailable until the first activity observation.");
            return i18n("Expected data: local activity estimates and any authenticated quota windows supported by this tool.");
        }
        var descriptions = {
            actual_usage_spend: i18n("Expected data: provider-reported usage and spend when the endpoint exposes them."),
            actual_key_usage: i18n("Expected data: actual API key usage; billing may be unavailable."),
            gateway_aggregate: i18n("Expected data: aggregate usage or spend reported by the gateway."),
            balance_connectivity: i18n("Expected data: account balance and connection status."),
            connectivity_only: i18n("Expected data: connection status only; usage metrics are not exposed by this source.")
        };
        return descriptions[sourceData.monitoringLevel]
            || i18n("Expected data: the capabilities documented for this source.");
    }

    function healthActionText() {
        if (retryBlocked)
            return i18n("The provider has asked us to wait until %1 before trying again.",
                        shortDate(sourceData.retryAfter));
        if (sourceData.readinessState === "waiting_for_activity")
            return i18n("No action is needed yet. Use the app to create its first local activity observation.");
        return sourceData.nextActionText || i18n("No action is required.");
    }

    function metricFreshnessText(state, observedAt) {
        var labels = {
            fresh: i18n("fresh"),
            aging: i18n("getting old"),
            stale: i18n("stale"),
            awaiting_refresh: i18n("awaiting refresh"),
            never_observed: i18n("not observed yet")
        };
        var label = labels[state] || i18n("freshness unknown");
        var observedMs = dateMilliseconds(observedAt);
        if (state === "fresh" || state === "never_observed" || observedMs <= 0)
            return label;
        return i18n("%1 · observed %2", label, localizedDate(observedAt));
    }

    function formatMetric(value, unit, currency) {
        if (value === undefined || value === null || value === "") return "\u2014";
        var number = Number(value);
        if (!Number.isFinite(number)) return "\u2014";
        if (unit === "percent_remaining")
            return i18n("%1% remaining", Math.round(number));
        if (currency)
            return i18n("%1 %2", currency,
                              number.toLocaleString(Qt.locale(), "f", 2));
        var integerUnit = /token|session|event/.test(unit || "");
        return i18n("%1 %2", integerUnit
            ? number.toLocaleString(Qt.locale(), "f", 0)
            : number.toLocaleString(Qt.locale()), unit);
    }

    function recentHistoryText() {
        var result = detailModel.recentHistory || {};
        if (!result.ok)
            return i18n("No compatible observations were retained in the last 7 days.");
        var series = result.series || [];
        var points = 0;
        var samples = 0;
        for (var i = 0; i < series.length; ++i) {
            points += Number(series[i].availablePointCount || 0);
            samples += Number(series[i].sampleCount || 0);
        }
        return i18np(
            "%1 recent point from %2 stored sample",
            "%1 recent points from %2 stored samples",
            points, samples);
    }

    function estimateProvenance() {
        return detail.sourceData.costProvenance || {};
    }

    function provenanceValue(key, fallback) {
        var value = estimateProvenance()[key];
        return value === undefined || value === null || value === ""
            ? fallback : String(value);
    }

    function catalogKey() {
        if (detail.mediaMode || detail.sourceData.sourceKind !== "provider") return "";
        return detail.sourceData.catalogKey || detail.sourceData.configKey
            || detail.sourceData.stableId || detail.sourceId;
    }

    function catalogModel() {
        var key = detail.catalogKey();
        var modelId = detail.sourceData.pricingModel || "";
        if (key === "" || modelId === "") return ({});
        return ProviderPricingCatalog.model(key, modelId) || ({});
    }

    function metricValue(kind) {
        var metrics = detail.sourceData.detailMetrics || [];
        for (var i = metrics.length - 1; i >= 0; --i) {
            if (metrics[i].kind === kind && metrics[i].available
                    && metrics[i].value !== undefined && metrics[i].value !== null)
                return Number(metrics[i].value);
        }
        return null;
    }

    function costLabRows() {
        var key = detail.catalogKey();
        var input = metricValue("input_tokens");
        var output = metricValue("output_tokens");
        if (key === "" || input === null || output === null) return [];

        var usage = { inputTokens: input, outputTokens: output };
        var cacheRead = metricValue("cache_read_input_tokens");
        var cacheWrite = metricValue("cache_creation_input_tokens");
        if (cacheRead !== null) usage.cachedInputTokens = cacheRead;
        if (cacheWrite !== null) usage.cacheWriteTokens = cacheWrite;
        if (detail.sourceData.pricingModality)
            usage.modality = detail.sourceData.pricingModality;
        if (detail.sourceData.pricingServiceTier)
            usage.serviceTier = detail.sourceData.pricingServiceTier;
        if (detail.sourceData.pricingRegion)
            usage.region = detail.sourceData.pricingRegion;
        if (detail.sourceData.pricingRoute)
            usage.route = detail.sourceData.pricingRoute;

        var models = ProviderPricingCatalog.selectableModelsForProvider(key) || [];
        var rows = [];
        for (var i = 0; i < models.length && rows.length < 8; ++i) {
            var modelId = models[i].id || "";
            if (modelId === "") continue;
            var estimate = ProviderPricingCatalog.estimateCost(key, modelId, usage) || ({});
            rows.push({
                id: modelId,
                displayName: models[i].displayName || modelId,
                current: modelId === detail.sourceData.pricingModel,
                available: !!estimate.available,
                amountText: estimate.amountText || "",
                currency: estimate.currency || "",
                missingDimensions: estimate.missingDimensions || []
            });
        }
        return rows;
    }

    function rateSummary(rates) {
        if (!rates) return "";
        var fields = [];
        if (rates.input !== undefined) fields.push(i18n("input %1", rates.input));
        if (rates.output !== undefined) fields.push(i18n("output %1", rates.output));
        if (rates.cacheWrite !== undefined) fields.push(i18n("cache write %1", rates.cacheWrite));
        return fields.join(i18n(" · "));
    }

    function sourceUrl() {
        var refs = detail.estimateProvenance().sourceRefs || detail.catalogModelData.sourceRefs || [];
        return refs.length > 0 ? (refs[0].url || "") : "";
    }

    function configureModel() {
        if (!detail.monitor) return;
        detailModel.registerDailyState(detail.monitor.presentationDailyState);
        detailModel.registerHistoryDatabase(detail.monitor.usageDb);
    }

    function runwayRows() {
        if (!forecastsEnabled) return [];
        if (mediaMode) {
            return [{
                kind: "budget_overrun",
                state: "warning",
                sourceId: sourceId,
                sourceKind: "provider",
                window: "calendar_month",
                scope: "organization",
                currentValue: 8.46,
                projectedValue: 18.72,
                limitValue: 15.0,
                unit: "USD",
                currency: "USD",
                contractVersion: "budget-pacing-v2",
                policyId: "media-openrouter-monthly",
                periodStart: "2026-07-01T00:00:00Z",
                predictedAt: "2026-07-29T10:00:00Z",
                periodEnd: "2026-08-01T00:00:00Z",
                spentMinor: 846,
                remainingMinor: 654,
                consumedPercent: 56.4,
                projectedPeriodEndMinor: 1872,
                predictedOverrun: true,
                safeTodayMinor: 32,
                remainingDailyAllowanceMinor: 55,
                previousPeriodSpentMinor: 762,
                previousPeriodChangePercent: 11.0,
                sampleCount: 20,
                coveragePercent: 80,
                evidenceGrade: "strong",
                methodId: "budget-pacing-v2",
                reasonKey: "",
                reasonText: "",
                valueClass: "actual"
            }];
        }
        var model = monitor ? monitor.guardrails : null;
        var forecasts = model ? model.forecasts || [] : [];
        var rows = [];
        for (var i = 0; i < forecasts.length; ++i) {
            if (forecasts[i].sourceId === sourceId)
                rows.push(forecasts[i]);
        }
        return rows;
    }

    function scopeLabel(row) {
        var labels = [];
        if (row.modelScopeAvailable)
            labels.push(i18n("Model: %1", row.modelScope));
        if (row.projectScopeAvailable)
            labels.push(row.projectDisplayKind === "deleted"
                ? i18n("Deleted project · …%1",
                       row.projectDisplaySuffix)
                : i18n("Project · …%1",
                       row.projectDisplaySuffix));
        if (row.serviceTierAvailable)
            labels.push(i18n("Service tier: %1", row.serviceTierScope));
        if (row.lineItemAvailable)
            labels.push(i18n("Line item: %1", row.lineItemScope));
        return labels.length > 0
            ? labels.join(i18n(" · ")) : i18n("Unattributed scope");
    }

    function scopeRows() {
        var rows = (detailModel.scopeBreakdown.scopedRows || []).slice();
        rows.sort(function(left, right) {
            return Number(right.value || 0) - Number(left.value || 0);
        });
        return showAllScopes ? rows : rows.slice(0, 8);
    }

    onSourceIdChanged: {
        showAllScopes = false;
        technicalDetailsExpanded = false;
        analysisExpanded = false;
        dataQualityExpanded = false;
    }

    onMonitorChanged: configureModel()

    Timer {
        interval: 30000
        repeat: true
        running: true
        onTriggered: detail.healthClockMs = Date.now()
    }

    ColumnLayout {
        width: detail.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.margins: Kirigami.Units.smallSpacing

            PlasmaComponents.ToolButton {
                id: backButton
                objectName: "sourceDetailBack"
                icon.name: "go-previous-symbolic"
                text: i18n("Back")
                activeFocusOnTab: true
                Accessible.name: i18n("Back to source list")
                onClicked: detail.backRequested()
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 0
                PlasmaExtras.Heading {
                    objectName: "sourceDetailTitle"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    level: 3
                    text: detail.sourceData.displayName || detail.sourceId
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: detail.statusText()
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }

            }
        }

        ColumnLayout {
            objectName: "sourceDetailHealthActions"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.margins: Kirigami.Units.smallSpacing
            visible: (detail.actionLabelData !== "" && detail.healthActionText() !== i18n("No action is required."))
                || detail.retryBlocked
                || ["stale", "never"].indexOf(detail.sourceData.freshnessState) >= 0
                || detail.sourceData.readinessState === "waiting_for_activity"
            PlasmaComponents.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: Number(detail.coverageData.availableMetricCount || 0)
                    < Number(detail.coverageData.totalMetricCount || 0)
                    && detail.healthActionText() !== i18n("No action is required.")
                text: i18np("Partial data · %1 of %2 metric available",
                            "Partial data · %1 of %2 metrics available",
                            Number(detail.coverageData.availableMetricCount || 0),
                            Number(detail.coverageData.totalMetricCount || 0))
                color: Kirigami.Theme.neutralTextColor
                wrapMode: Text.WordWrap
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: detail.healthActionText()
                wrapMode: Text.WordWrap
                Accessible.name: detail.healthActionText()
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: detail.actionLabelData !== ""
                PlasmaComponents.Button {
                    objectName: "sourceDetailPrimaryAction"
                    text: detail.retryBlocked
                        ? i18n("Retry at %1", detail.shortDate(detail.sourceData.retryAfter))
                        : detail.actionLabelData
                    icon.name: detail.retryBlocked
                        ? "chronometer"
                        : text === i18n("Refresh")
                            ? "view-refresh" : "configure"
                    activeFocusOnTab: true
                    enabled: !detail.retryBlocked
                    Accessible.name: text
                    Accessible.description: detail.healthActionText()
                    onClicked: {
                        if (!SourceHealth.canRequestAction(
                                    detail.sourceData.retryAfter,
                                    detail.healthClockMs)) return;
                        detail.actionRequested(
                            detail.sourceId,
                            detail.sourceData.nextActionKey || "open_source_settings",
                            detail.sourceData.sourceKind || "");
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        Repeater {
            model: detail.warningRunwayData

            Monitor.RunwayCard {
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                forecast: modelData
                compact: true
                showHistoryAction: detailModel.historyId !== ""
                onHistoryRequested: function(metric) {
                    detail.historyRequested(
                        detailModel.historyId, metric, 30);
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Price change inbox")
            visible: !detail.mediaMode && Object.keys(detail.priceChangeData).length > 0
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: !detail.mediaMode && Object.keys(detail.priceChangeData).length > 0
            implicitHeight: priceChangeLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: Kirigami.Units.smallSpacing
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.2)

            ColumnLayout {
                id: priceChangeLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: detail.priceChangeData.summary || i18n("A reviewed catalog change affects this model.")
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: i18n("Effective %1 · previous: %2 · current: %3",
                               detail.priceChangeData.effectiveDate || i18n("review date unknown"),
                               detail.rateSummary(detail.priceChangeData.previous),
                               detail.rateSummary(detail.priceChangeData.current))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Quota windows")
            visible: detail.quotaData.length > 0
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: text !== ""
            wrapMode: Text.WordWrap
            text: {
                var retained = detail.sourceData.lastKnownQuotaWindows || [];
                var lines = [];
                for (var i = 0; i < retained.length; ++i) {
                    var row = retained[i];
                    if (row.available !== false || !row.observedAt) continue;
                    var value = row.percentRemaining;
                    var unit = row.unit || i18n("% remaining");
                    if (value === undefined && typeof row.percentUsed === "number")
                        value = 100 - row.percentUsed;
                    if (value === undefined && row.lastKnownAvailable)
                        value = row.lastKnownValue;
                    if (value === undefined) value = row.value;
                    if (typeof value !== "number" || !Number.isFinite(value)) continue;
                    lines.push(i18n("Last known · %1: %2 %3 · observed %4 · awaiting fresh data",
                        row.window || row.label || row.kind, value, unit,
                        detail.shortDate(row.observedAt)));
                }
                return lines.join("\n");
            }
            Accessible.name: text
        }

        Repeater {
            model: detail.quotaData
            Rectangle {
                id: quotaRow
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                implicitHeight: quotaContent.implicitHeight
                    + Kirigami.Units.mediumSpacing * 2
                radius: Kirigami.Units.smallSpacing
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
                border.width: 1
                border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.18)

                ColumnLayout {
                    id: quotaContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: detail.quotaLabel(quotaRow.modelData)
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        PlasmaComponents.Label {
                            text: detail.quotaRemaining(quotaRow.modelData) !== null
                                ? i18n("%1% remaining", Math.round(detail.quotaRemaining(quotaRow.modelData)))
                                : i18n("Unavailable")
                        }
                    }
                    PlasmaComponents.ProgressBar {
                        objectName: "sourceDetailQuotaRemaining"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        from: 0
                        to: 100
                        visible: detail.quotaRemaining(quotaRow.modelData) !== null
                        value: detail.quotaRemaining(quotaRow.modelData) || 0
                        Accessible.name: i18n("%1 quota remaining", detail.quotaLabel(quotaRow.modelData))
                        Accessible.description: i18n("%1% remaining", Math.round(value))
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: [quotaRow.modelData.sourceClass === "actual"
                                   ? i18n("Provider-reported")
                                   : quotaRow.modelData.sourceClass === "estimated"
                                       ? i18n("Estimated") : quotaRow.modelData.sourceClass || "",
                               detail.dateMilliseconds(quotaRow.modelData.resetAt) > 0
                                   ? Utils.formatResetCountdown(quotaRow.modelData.resetAt) : ""]
                            .filter(function(value) { return !!value; }).join(i18n(" · "))
                        visible: text !== ""
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }


        ColumnLayout {
            objectName: "sourceDetailUsageSummary"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.margins: Kirigami.Units.smallSpacing
            visible: !detail.hasSessionSummary
                && !(detail.quotaData.length > 0
                     && detail.sourceData.primaryMetricUnit === "percent_remaining")
            PlasmaExtras.Heading {
                level: 4
                text: i18n("Usage summary")
            }
            PlasmaExtras.Heading {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                level: 3
                text: detail.sourceData.primaryMetricAvailable
                    ? detail.summaryMetric(detail.sourceData.primaryMetricValue,
                                          detail.sourceData.primaryMetricUnit,
                                          detail.sourceData.primaryMetricCurrency)
                    : i18n("Unavailable")
                activeFocusOnTab: true
                Accessible.description: detail.sourceData.primaryMetricAvailable
                    ? detail.formatMetric(detail.sourceData.primaryMetricValue,
                                          detail.sourceData.primaryMetricUnit,
                                          detail.sourceData.primaryMetricCurrency)
                    : i18n("Unavailable")
                QQC2.ToolTip.visible: hovered || activeFocus
                QQC2.ToolTip.text: Accessible.description
                property bool hovered: primaryHover.hovered
                HoverHandler { id: primaryHover }
                wrapMode: Text.WordWrap
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: [(detail.sourceData.primaryMetricKind || "").replace(/_/g, " "),
                       detail.sourceData.primaryMetricWindow || ""]
                    .filter(function(value) { return !!value; }).join(i18n(" · "))
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.WordWrap
            }
        }

        Components.LocalSessionPanel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: detail.hasSessionSummary
            sessionModel: detail.monitor ? detail.monitor.localSessions : null
            fixedToolId: detail.sourceId
            sessionState: detail.sessionState
            presentationMode: "summary"
            onSessionHistoryRequested: function(toolId, projectId, rangeDays,
                                                sessionId, pageOffset, modelFilter) {
                detail.sessionHistoryRequested(toolId, projectId, rangeDays,
                                               sessionId, pageOffset, modelFilter);
            }
        }

        PlasmaComponents.Button {
            objectName: "sourceDetailAnalysisToggle"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            text: i18n("Analysis")
            icon.name: detail.analysisExpanded ? "arrow-down" : "arrow-right"
            checkable: true
            checked: detail.analysisExpanded
            activeFocusOnTab: true
            Accessible.name: text
            onClicked: detail.analysisExpanded = checked
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Local Cost Lab")
            visible: detail.analysisExpanded && !detail.mediaMode && detail.costLabData.length > 0
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: detail.analysisExpanded && !detail.mediaMode && detail.costLabData.length > 0
            implicitHeight: costLabLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: Kirigami.Units.smallSpacing
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.14)

            ColumnLayout {
                id: costLabLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: i18n("Hypothetical list-price comparison for this source's retained workload. It is not a quality recommendation or a provider bill.")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: detail.costLabData
                    delegate: RowLayout {
                        id: costLabRow
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: costLabRow.modelData.displayName
                                + (costLabRow.modelData.current ? i18n(" · current") : "")
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            text: costLabRow.modelData.available
                                ? i18n("%1 %2", costLabRow.modelData.currency, costLabRow.modelData.amountText)
                                : i18n("Unavailable")
                            color: costLabRow.modelData.available
                                ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }
                    }
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Runway")
            visible: detail.analysisExpanded && detail.forecastsEnabled
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: detail.analysisExpanded && detail.forecastsEnabled
                && detail.runwayData.length === 0
                && !(detail.monitor && detail.monitor.guardrails
                     && detail.monitor.guardrails.busy)
            type: Kirigami.MessageType.Information
            text: i18n("Runway is unavailable until compatible local history meets the fixed sample, time, and coverage rules.")
            Accessible.name: text
        }

        PlasmaComponents.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: detail.analysisExpanded && detail.forecastsEnabled
                && detail.monitor && detail.monitor.guardrails
                && detail.monitor.guardrails.busy
                && detail.runwayData.length === 0
            running: visible
        }

        Repeater {
            model: detail.analysisExpanded ? detail.analysisRunwayData : []

            Monitor.RunwayCard {
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                forecast: modelData
                showHistoryAction: detailModel.historyId !== ""
                onHistoryRequested: function(metric) {
                    detail.historyRequested(
                        detailModel.historyId, metric, 30);
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Reported scope breakdown")
            visible: detail.analysisExpanded && detail.scopeData.length > 0
        }

        Repeater {
            model: detail.analysisExpanded ? detail.scopeData : []

            Rectangle {
                id: scopeRow
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                implicitHeight: scopeContent.implicitHeight
                    + Kirigami.Units.mediumSpacing * 2
                radius: Kirigami.Units.smallSpacing
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
                border.width: 1
                border.color: Qt.alpha(Kirigami.Theme.textColor, 0.12)
                Accessible.role: Accessible.Grouping
                Accessible.name: detail.scopeLabel(scopeRow.modelData)

                ColumnLayout {
                    id: scopeContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing

                    GridLayout {
                        columns: detail.availableWidth >= Kirigami.Units.gridUnit * 40 ? 2 : 1
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: detail.scopeLabel(scopeRow.modelData)
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            text: detail.formatMetric(
                                scopeRow.modelData.value,
                                scopeRow.modelData.unit,
                                scopeRow.modelData.currency)
                        }
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: i18n("%1 · %2 · provider-reported scope",
                                   scopeRow.modelData.kind
                                       .replace(/_/g, " "),
                                   scopeRow.modelData.valueClass)
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        PlasmaComponents.Button {
            Layout.alignment: Qt.AlignHCenter
            visible: detail.analysisExpanded && detail.totalScopeRows > 8 && !detail.showAllScopes
            text: i18np("Show all %1 scope", "Show all %1 scopes",
                        detail.totalScopeRows)
            icon.name: "view-list-details"
            activeFocusOnTab: true
            Accessible.name: text
            onClicked: detail.showAllScopes = true
        }

        PlasmaExtras.Heading {
            visible: detail.analysisExpanded
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Recent compatible history")
        }

        RowLayout {
            visible: detail.analysisExpanded
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.BusyIndicator {
                visible: !detail.mediaMode && detailModel.historyLoading
                running: visible
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: width
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: detail.mediaMode
                    ? i18n("24 recent points from 96 stored samples")
                    : detailModel.historyLoading
                    ? i18n("Loading recent compatible history…")
                    : detail.recentHistoryText()
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                Accessible.name: text
            }
        }

        PlasmaComponents.Button {
            objectName: "sourceDetailQualityToggle"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            text: i18n("Data quality")
            icon.name: detail.dataQualityExpanded ? "arrow-down" : "arrow-right"
            checkable: true
            checked: detail.dataQualityExpanded
            activeFocusOnTab: true
            Accessible.name: text
            onClicked: detail.dataQualityExpanded = checked
        }

        Rectangle {
            visible: detail.dataQualityExpanded
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            implicitHeight: statusLayout.implicitHeight
                + Kirigami.Units.largeSpacing * 2
            radius: Kirigami.Units.cornerRadius
            color: Qt.alpha(Kirigami.Theme.highlightColor, 0.08)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.24)

            ColumnLayout {
                id: statusLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: i18np(
                        "%1 available metric of %2",
                        "%1 available metrics of %2",
                        Number(detail.coverageData.availableMetricCount || 0),
                        Number(detail.coverageData.totalMetricCount || 0))
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: i18n("Provenance and data quality are shown for every metric. Missing values stay unavailable.")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: detail.expectedCapabilityText()
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: i18n("Last attempt: %1", detail.localizedDate(detail.sourceData.lastAttempt))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: i18n("Last successful check: %1", detail.localizedDate(detail.sourceData.lastSuccess))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    visible: detail.dateMilliseconds(detail.sourceData.nextScheduledRefresh) > 0
                    text: i18n("Next scheduled check: %1", detail.localizedDate(detail.sourceData.nextScheduledRefresh))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Why this estimate?")
            visible: detail.dataQualityExpanded && !detail.mediaMode && Object.keys(detail.estimateProvenance()).length > 0
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: detail.dataQualityExpanded && !detail.mediaMode && Object.keys(detail.estimateProvenance()).length > 0
            implicitHeight: estimateLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: Kirigami.Units.smallSpacing
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.14)

            ColumnLayout {
                id: estimateLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: detail.estimateProvenance().available
                        ? i18n("Available from the verified local price book.")
                        : i18n("Unavailable: no trustworthy price was applied.")
                    font.bold: true
                    color: detail.estimateProvenance().available
                        ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: [
                        detail.provenanceValue("modelId", detail.sourceData.pricingModel || ""),
                        detail.provenanceValue("catalogVersion", ""),
                        detail.provenanceValue("priceId", "")
                    ].filter(function(value) { return !!value; }).join(i18n(" · "))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    visible: (detail.estimateProvenance().missingDimensions || []).length > 0
                    text: i18n("Missing: %1",
                        (detail.estimateProvenance().missingDimensions || []).join(i18n(", ")))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        PlasmaComponents.Button {
            Layout.leftMargin: Kirigami.Units.smallSpacing
            visible: detail.dataQualityExpanded && detail.sourceUrl() !== ""
            text: i18n("Open official pricing source")
            icon.name: "internet-web-browser"
            activeFocusOnTab: true
            onClicked: Qt.openUrlExternally(detail.sourceUrl())
        }

        PlasmaComponents.Button {
            objectName: "sourceDetailTechnicalToggle"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            text: i18n("Technical metrics")
            icon.name: detail.technicalDetailsExpanded ? "arrow-down" : "arrow-right"
            checkable: true
            checked: detail.technicalDetailsExpanded
            activeFocusOnTab: true
            Accessible.name: text
            Accessible.description: i18n("Shows or hides per-metric values, freshness, and provenance.")
            onClicked: detail.technicalDetailsExpanded = checked
        }

        Repeater {
            model: !detail.mediaMode && detail.technicalDetailsExpanded ? detailModel : null
            visible: !detail.mediaMode && detail.technicalDetailsExpanded
            Rectangle {
                id: metricRow
                required property string kind
                required property bool available
                required property var value
                required property string unit
                required property string currency
                required property string source
                required property string quality
                required property string semantic
                required property string scope
                required property string window
                required property var lastKnownValue
                required property bool lastKnownAvailable
                required property string freshnessState
                required property var observedAt
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                implicitHeight: metricContent.implicitHeight
                    + Kirigami.Units.mediumSpacing * 2
                radius: Kirigami.Units.smallSpacing
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
                border.width: 1
                border.color: Qt.alpha(Kirigami.Theme.textColor, 0.12)

                ColumnLayout {
                    id: metricContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    GridLayout {
                        columns: detail.availableWidth >= Kirigami.Units.gridUnit * 40 ? 2 : 1
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: metricRow.kind.replace(/_/g, " ")
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            text: metricRow.available
                                ? detail.formatMetric(metricRow.value,
                                                      metricRow.unit,
                                                      metricRow.currency)
                                : metricRow.lastKnownAvailable
                                    ? i18n("Last known: %1", detail.formatMetric(
                                        metricRow.lastKnownValue,
                                        metricRow.unit,
                                        metricRow.currency))
                                    : i18n("Unavailable")
                            color: metricRow.available || metricRow.lastKnownAvailable
                                ? Kirigami.Theme.textColor
                                : Kirigami.Theme.disabledTextColor
                        }
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: [metricRow.source, metricRow.quality,
                               metricRow.semantic, metricRow.scope,
                               metricRow.window,
                               detail.metricFreshnessText(
                                   metricRow.freshnessState,
                                   metricRow.observedAt)
                              ].filter(function(value) {
                                   return !!value;
                               }).join(i18n(" · "))
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.WordWrap
                        Accessible.name: i18n("Metric provenance: %1", text)
                    }
                }
            }
        }

        Repeater {
            model: detail.technicalDetailsExpanded ? detail.mediaMetrics : []
            visible: detail.technicalDetailsExpanded
            Rectangle {
                id: mediaMetricRow
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                implicitHeight: mediaMetricContent.implicitHeight
                    + Kirigami.Units.mediumSpacing * 2
                radius: Kirigami.Units.smallSpacing
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
                border.width: 1
                border.color: Qt.alpha(Kirigami.Theme.textColor, 0.12)

                ColumnLayout {
                    id: mediaMetricContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    GridLayout {
                        columns: detail.availableWidth >= Kirigami.Units.gridUnit * 40 ? 2 : 1
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: mediaMetricRow.modelData.kind.replace(/_/g, " ")
                            font.bold: true
                        }
                        PlasmaComponents.Label {
                            text: mediaMetricRow.modelData.available
                                ? detail.formatMetric(
                                    mediaMetricRow.modelData.value,
                                    mediaMetricRow.modelData.unit,
                                    mediaMetricRow.modelData.currency)
                                : i18n("Unavailable")
                            color: mediaMetricRow.modelData.available
                                ? Kirigami.Theme.textColor
                                : Kirigami.Theme.disabledTextColor
                        }
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: [
                            mediaMetricRow.modelData.source,
                            mediaMetricRow.modelData.quality,
                            mediaMetricRow.modelData.semantic,
                            mediaMetricRow.modelData.scope,
                            mediaMetricRow.modelData.window
                        ].filter(function(value) { return !!value; })
                            .join(i18n(" · "))
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.WordWrap
                        Accessible.name: i18n("Metric provenance: %1", text)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Button {
                objectName: "sourceDetailSettings"
                text: i18n("Open source settings")
                icon.name: "configure"
                activeFocusOnTab: true
                onClicked: detail.settingsRequested(detail.sourceId)
            }
            PlasmaComponents.Button {
                objectName: "sourceDetailHistory"
                text: i18n("Open compatible history")
                icon.name: "view-history"
                activeFocusOnTab: true
                enabled: detailModel.historyMetric !== ""
                onClicked: detail.historyRequested(
                    detailModel.historyId, detailModel.historyMetric, 7)
            }
            Item { Layout.fillWidth: true }
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }

    Component.onCompleted: {
        configureModel();
        backButton.forceActiveFocus();
    }
}
