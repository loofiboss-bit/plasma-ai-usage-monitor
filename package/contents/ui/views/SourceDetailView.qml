pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.ki18n
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0

QQC2.ScrollView {
    id: detail

    property var monitor: null
    property string sourceId: ""
    signal backRequested()
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    signal settingsRequested(string stableId)
    signal historyRequested(string historyId, string metric, int rangeDays)
    Accessible.role: Accessible.Pane
    Accessible.name: detailModel.source.displayName
        ? KI18n.i18n("%1 source details", detailModel.source.displayName)
        : KI18n.i18n("Source details")

    SourceDetailModel {
        id: detailModel
        sourceId: detail.sourceId
    }

    function statusText() {
        var labels = {
            actual: KI18n.i18n("Provider-reported data"),
            estimated: KI18n.i18n("Local estimate"),
            balance: KI18n.i18n("Account balance"),
            connectivity_only: KI18n.i18n("Connectivity only"),
            unavailable: KI18n.i18n("Data unavailable")
        };
        return labels[detailModel.source.qualityClass]
            || KI18n.i18n("Data unavailable");
    }

    function formatMetric(value, unit, currency) {
        if (value === undefined || value === null) return "\u2014";
        var number = Number(value);
        if (!Number.isFinite(number)) return "\u2014";
        if (unit === "percent_remaining")
            return KI18n.i18n("%1% remaining", Math.round(number));
        if (currency)
            return KI18n.i18n("%1 %2", currency,
                              number.toLocaleString(Qt.locale(), "f", 2));
        return KI18n.i18n("%1 %2", number.toLocaleString(Qt.locale()), unit);
    }

    function recentHistoryText() {
        var result = detailModel.recentHistory || {};
        if (!result.ok)
            return KI18n.i18n("No compatible observations were retained in the last 7 days.");
        var series = result.series || [];
        var points = 0;
        var samples = 0;
        for (var i = 0; i < series.length; ++i) {
            points += Number(series[i].availablePointCount || 0);
            samples += Number(series[i].sampleCount || 0);
        }
        return KI18n.i18np(
            "%1 recent point from %2 stored sample",
            "%1 recent points from %2 stored samples",
            points, samples);
    }

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
                text: KI18n.i18n("Back")
                activeFocusOnTab: true
                Accessible.name: KI18n.i18n("Back to source list")
                onClicked: detail.backRequested()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                PlasmaExtras.Heading {
                    objectName: "sourceDetailTitle"
                    Layout.fillWidth: true
                    level: 3
                    text: detailModel.source.displayName || detail.sourceId
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: detail.statusText()
                        + KI18n.i18n(" · %1", detailModel.source.freshnessState
                                    || KI18n.i18n("unknown freshness"))
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
                    text: KI18n.i18np(
                        "%1 available metric of %2",
                        "%1 available metrics of %2",
                        Number(detailModel.coverage.availableMetricCount || 0),
                        Number(detailModel.coverage.totalMetricCount || 0))
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18n("Provenance and data quality are shown for every metric. Missing values stay unavailable.")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: detailModel.actionLabel !== ""
                    PlasmaComponents.Button {
                        objectName: "sourceDetailPrimaryAction"
                        text: detailModel.actionLabel
                        icon.name: text === KI18n.i18n("Refresh")
                            ? "view-refresh" : "configure"
                        activeFocusOnTab: true
                        onClicked: detail.actionRequested(
                            detail.sourceId,
                            detailModel.source.nextActionKey || "",
                            detailModel.source.sourceKind || "")
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
            text: KI18n.i18n("Quota windows")
            visible: detailModel.quotaWindows.length > 0
        }

        Repeater {
            model: detailModel.quotaWindows
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
                        text: KI18n.i18n("%1% remaining",
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
            text: KI18n.i18n("Typed metrics")
        }

        Repeater {
            model: detailModel
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
                                : KI18n.i18n("Unavailable")
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
                               }).join(KI18n.i18n(" · "))
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        Accessible.name: KI18n.i18n("Metric provenance: %1", text)
                    }
                }
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            level: 4
            text: KI18n.i18n("Recent compatible history")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.BusyIndicator {
                visible: detailModel.historyLoading
                running: visible
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: width
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: detailModel.historyLoading
                    ? KI18n.i18n("Loading recent compatible history…")
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
                text: KI18n.i18n("Open source settings")
                icon.name: "configure"
                activeFocusOnTab: true
                onClicked: detail.settingsRequested(detail.sourceId)
            }
            PlasmaComponents.Button {
                objectName: "sourceDetailHistory"
                text: KI18n.i18n("Open compatible history")
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
        detailModel.registerDailyState(detail.monitor.presentationDailyState);
        detailModel.registerHistoryDatabase(detail.monitor.usageDb);
        backButton.forceActiveFocus();
    }
}
