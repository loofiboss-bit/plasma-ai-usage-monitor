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

QQC2.ScrollView {
    id: detail

    property var monitor: null
    property string sourceId: ""
    readonly property bool mediaMode: AppInfo.demoMode
        && AppInfo.smokeView === "media-source-detail"
    readonly property var mediaSource: mediaMode && monitor
        ? monitor.presentationDailyState.source(sourceId) : ({})
    readonly property var sourceData: mediaMode ? mediaSource : detailModel.source
    readonly property var quotaData: mediaMode
        ? (mediaSource.quotaWindows || []) : detailModel.quotaWindows
    readonly property var coverageData: mediaMode
        ? ({ availableMetricCount: 2, totalMetricCount: 3 })
        : detailModel.coverage
    readonly property string actionLabelData: mediaMode
        ? i18n("Open source settings") : detailModel.actionLabel
    readonly property var mediaMetrics: mediaMode
        ? (mediaSource.detailMetrics || []) : []
    readonly property var runwayData: runwayRows()
    readonly property var scopeData: mediaMode
        ? [] : scopeRows()
    signal backRequested()
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    signal settingsRequested(string stableId)
    signal historyRequested(string historyId, string metric, int rangeDays)
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
        return labels[sourceData.qualityClass]
            || i18n("Data unavailable");
    }

    function formatMetric(value, unit, currency) {
        if (value === undefined || value === null) return "\u2014";
        var number = Number(value);
        if (!Number.isFinite(number)) return "\u2014";
        if (unit === "percent_remaining")
            return i18n("%1% remaining", Math.round(number));
        if (currency)
            return i18n("%1 %2", currency,
                              number.toLocaleString(Qt.locale(), "f", 2));
        return i18n("%1 %2", number.toLocaleString(Qt.locale()), unit);
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

    function configureModel() {
        if (!detail.monitor) return;
        detailModel.registerDailyState(detail.monitor.presentationDailyState);
        detailModel.registerHistoryDatabase(detail.monitor.usageDb);
    }

    function runwayRows() {
        if (!Plasmoid.configuration.forecastUiEnabled) return [];
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
                predictedAt: "2026-07-29T10:00:00Z",
                periodEnd: "2026-08-01T00:00:00Z",
                sampleCount: 20,
                coveragePercent: 80,
                evidenceGrade: "strong",
                methodId: "budget-pacing-v1",
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
        return rows.slice(0, 8);
    }

    onMonitorChanged: configureModel()

    ColumnLayout {
        width: detail.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Layout.fillWidth: true
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
                spacing: 0
                PlasmaExtras.Heading {
                    objectName: "sourceDetailTitle"
                    Layout.fillWidth: true
                    level: 3
                    text: detail.sourceData.displayName || detail.sourceId
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: detail.statusText()
                        + i18n(" · %1", detail.sourceData.freshnessState
                                    || i18n("unknown freshness"))
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
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
                    text: i18np(
                        "%1 available metric of %2",
                        "%1 available metrics of %2",
                        Number(detail.coverageData.availableMetricCount || 0),
                        Number(detail.coverageData.totalMetricCount || 0))
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Provenance and data quality are shown for every metric. Missing values stay unavailable.")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: detail.actionLabelData !== ""
                    PlasmaComponents.Button {
                        objectName: "sourceDetailPrimaryAction"
                        text: detail.actionLabelData
                        icon.name: text === i18n("Refresh")
                            ? "view-refresh" : "configure"
                        activeFocusOnTab: true
                        onClicked: detail.actionRequested(
                            detail.sourceId,
                            detail.sourceData.nextActionKey || "open_source_settings",
                            detail.sourceData.sourceKind || "")
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Quota windows")
            visible: detail.quotaData.length > 0
        }

        Repeater {
            model: detail.quotaData
            Rectangle {
                id: quotaRow
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                implicitHeight: quotaContent.implicitHeight
                    + Kirigami.Units.mediumSpacing * 2
                radius: Kirigami.Units.smallSpacing
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
                border.width: 1
                border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.18)

                RowLayout {
                    id: quotaContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: quotaRow.modelData.window || quotaRow.modelData.kind
                        font.bold: true
                    }
                    PlasmaComponents.Label {
                        text: i18n("%1% remaining",
                            Math.round(Number(quotaRow.modelData.percentRemaining)))
                    }
                    PlasmaComponents.Label {
                        text: quotaRow.modelData.sourceClass || ""
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Runway")
            visible: Plasmoid.configuration.forecastUiEnabled
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: Plasmoid.configuration.forecastUiEnabled
                && detail.runwayData.length === 0
                && !(detail.monitor && detail.monitor.guardrails
                     && detail.monitor.guardrails.busy)
            type: Kirigami.MessageType.Information
            text: i18n("Runway is unavailable until compatible local history meets the fixed sample, time, and coverage rules.")
            Accessible.name: text
        }

        PlasmaComponents.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: Plasmoid.configuration.forecastUiEnabled
                && detail.monitor && detail.monitor.guardrails
                && detail.monitor.guardrails.busy
                && detail.runwayData.length === 0
            running: visible
        }

        Repeater {
            model: detail.runwayData

            Monitor.RunwayCard {
                required property var modelData
                Layout.fillWidth: true
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
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Typed metrics")
        }

        Repeater {
            model: detailModel
            visible: !detail.mediaMode
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
                Layout.fillWidth: true
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
                    RowLayout {
                        Layout.fillWidth: true
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: metricRow.kind.replace(/_/g, " ")
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            text: metricRow.available
                                ? detail.formatMetric(metricRow.value,
                                                      metricRow.unit,
                                                      metricRow.currency)
                                : i18n("Unavailable")
                            color: metricRow.available
                                ? Kirigami.Theme.textColor
                                : Kirigami.Theme.disabledTextColor
                        }
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: [metricRow.source, metricRow.quality,
                               metricRow.semantic, metricRow.scope,
                               metricRow.window].filter(function(value) {
                                   return !!value;
                               }).join(i18n(" · "))
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        Accessible.name: i18n("Metric provenance: %1", text)
                    }
                }
            }
        }

        Repeater {
            model: detail.mediaMetrics
            Rectangle {
                id: mediaMetricRow
                required property var modelData
                Layout.fillWidth: true
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
                    RowLayout {
                        Layout.fillWidth: true
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
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
                        elide: Text.ElideRight
                        Accessible.name: i18n("Metric provenance: %1", text)
                    }
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Reported scope breakdown")
            visible: detail.scopeData.length > 0
        }

        Repeater {
            model: detail.scopeData

            Rectangle {
                id: scopeRow
                required property var modelData
                Layout.fillWidth: true
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

                    RowLayout {
                        Layout.fillWidth: true
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
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

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18n("Recent compatible history")
        }

        RowLayout {
            Layout.fillWidth: true
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

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing

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
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }

    Component.onCompleted: {
        configureModel();
        backButton.forceActiveFocus();
    }
}
