pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import ".." as Monitor
import "../components" as Components

ColumnLayout {
    id: history

    property var monitor: null
    property string pendingExportFormat: "csv"
    property string exportStatus: ""
    property string requestedSourceId: ""
    property string requestedMetric: ""
    property int requestedRangeDays: 7
    property bool sessionsMode: false
    property string sessionToolId: "codex-cli"
    property string sessionProjectId: ""
    property var sessionState: null
    property var catalogDatabase: null
    Accessible.name: historyController.loading
        ? i18n("History view loading")
        : i18n("History view ready")

    Components.HistoryController {
        id: historyController
        objectName: "historyController"
        usageDb: history.monitor ? history.monitor.usageDb : null
        dailyState: history.monitor ? history.monitor.presentationDailyState : null
        configuredProviders: history.monitor ? history.monitor.allProviders || [] : []
        configuredTools: history.monitor ? history.monitor.allSubscriptionTools || [] : []
    }

    function refreshHistoryCatalog(force) {
        var database = history.monitor ? history.monitor.usageDb : null;
        if (!database) return;
        if (!force && history.catalogDatabase === database) return;
        history.catalogDatabase = database;
        historyController.refreshCatalog();
    }

    function scrollToTop() {
        Qt.callLater(function() {
            // Controls exposes a Flickable here at runtime, but its qmltypes only declare QQuickItem.
            // qmllint disable missing-property
            historyScroll.contentItem.contentY = 0;
            // qmllint enable missing-property
        });
    }

    function emptyStateTitle() {
        if (historyController.errorKey !== "")
            return i18n("This comparison is not compatible");
        return historyController.metricRows().length === 0
            ? i18n("No usage metrics have been retained")
            : i18n("No observations in this period");
    }

    function emptyStateDetails() {
        if (historyController.errorKey !== "")
            return historyController.errorText();
        if (historyController.metricRows().length === 0) {
            if (!historyController.usageDb || !historyController.usageDb.enabled)
                return i18n("Enable usage history in Settings → History. New metrics appear after the next saved observation.");
            return i18n("This source has no saved usage snapshots yet. Refresh the source and return after its next snapshot is stored.");
        }
        return i18n("No observations for this metric in this period. Try a wider range or refresh the source.");
    }

    Timer {
        id: mediaHistorySelection
        interval: 500
        repeat: true
        running: AppInfo.demoMode
            && AppInfo.smokeView.indexOf("media-history") === 0
        property int attempts: 0
        onTriggered: {
            attempts++;
            var rows = historyController.sourceRows || [];
            for (var i = 0; i < rows.length; ++i) {
                if (rows[i].dbName !== "OpenAI" || !rows[i].hasHistory)
                    continue;
                historyController.selectedSourceId = rows[i].historyId;
                historyController.selectedMetric = "cost";
                historyController.rangeIndex = 1;
                if (!historyController.loading
                        && historyController.seriesData.length === 0)
                    historyController.refresh();
                if (historyController.seriesData.length > 0)
                    stop();
                return;
            }
            if (attempts % 2 === 0)
                historyController.refreshCatalog();
            if (attempts >= 20)
                stop();
        }
    }

    Timer {
        id: catalogRefreshTimer
        interval: 400
        repeat: false
        onTriggered: history.refreshHistoryCatalog(true)
    }

    Connections {
        target: history.monitor ? history.monitor.usageDb : null
        enabled: target !== null
        ignoreUnknownSignals: true

        function onObservationsChanged() {
            if (!history.sessionsMode)
                catalogRefreshTimer.restart();
        }
    }

    Connections {
        target: history.monitor
        enabled: history.monitor !== null
        ignoreUnknownSignals: true

        function onUsageDbChanged() {
            history.refreshHistoryCatalog(true);
        }
    }

    function openExport(format) {
        pendingExportFormat = format;
        exportDialog.nameFilters = format === "json"
            ? [i18n("JSON files (*.json)")] : [i18n("CSV files (*.csv)")];
        exportDialog.currentFile = "ai-usage-history." + format;
        exportDialog.open();
    }

    function applyRequestedSelection() {
        if (!requestedSourceId) return;
        historyController.applyDeepLink(requestedSourceId, requestedMetric,
                                        requestedRangeDays);
    }

    Connections {
        target: historyController
        function onCatalogAccepted() {
            history.applyRequestedSelection();
        }
    }

    onMonitorChanged: Qt.callLater(function() {
        history.refreshHistoryCatalog(false);
    })
    onSessionsModeChanged: {
        scrollToTop();
        if (!sessionsMode)
            catalogRefreshTimer.restart();
    }
    onRequestedSourceIdChanged: Qt.callLater(applyRequestedSelection)
    onRequestedMetricChanged: Qt.callLater(applyRequestedSelection)
    onRequestedRangeDaysChanged: Qt.callLater(applyRequestedSelection)

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents.ToolButton {
            objectName: "historyUsageMode"
            text: i18n("Usage")
            icon.name: "office-chart-line"
            display: QQC2.AbstractButton.TextBesideIcon
            checkable: true
            autoExclusive: true
            checked: !history.sessionsMode
            activeFocusOnTab: true
            onClicked: history.sessionsMode = false
        }
        PlasmaComponents.ToolButton {
            objectName: "historySessionsMode"
            text: i18n("Sessions")
            icon.name: "view-history"
            display: QQC2.AbstractButton.TextBesideIcon
            checkable: true
            autoExclusive: true
            checked: history.sessionsMode
            activeFocusOnTab: true
            Accessible.name: i18n("Show coding session history")
            onClicked: history.sessionsMode = true
        }
        Item { Layout.fillWidth: true }
    }

    QQC2.ScrollView {
        id: historyScroll
        objectName: "historyContentScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 0
        clip: true
        contentWidth: availableWidth
        contentHeight: historyContent.implicitHeight
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ColumnLayout {
            id: historyContent
            objectName: "historyContent"
            width: historyScroll.availableWidth
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                visible: !history.sessionsMode
                PlasmaComponents.ToolButton {
                    objectName: "historySingleSourceMode"
                    text: i18n("Single source")
                    checkable: true
                    autoExclusive: true
                    checked: !historyController.compareMode
                    activeFocusOnTab: true
                    onClicked: {
                        historyController.compareMode = false;
                        historyController.normalizeSelection();
                        Qt.callLater(historyController.refresh);
                    }
                }
                PlasmaComponents.ToolButton {
                    objectName: "historyCompareMode"
                    Accessible.name: i18n("Show comparison history")
                    text: i18n("Compare")
                    checkable: true
                    autoExclusive: true
                    checked: historyController.compareMode
                    activeFocusOnTab: true
                    onClicked: {
                        historyController.compareMode = true;
                        historyController.normalizeSelection();
                        Qt.callLater(historyController.refresh);
                    }
                }
                Item { Layout.fillWidth: true }
            }

            GridLayout {
                id: historyControls
                objectName: "historyControls"
                Layout.fillWidth: true
                visible: !history.sessionsMode
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                columns: history.width < Kirigami.Units.gridUnit * 40 ? 1 : 5
                columnSpacing: Kirigami.Units.smallSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    id: rangeCombo
                    objectName: "historyRangeCombo"
                    Layout.fillWidth: true
                    model: [i18n("24 hours"), i18n("7 days"), i18n("30 days"), i18n("90 days")]
                    currentIndex: historyController.rangeIndex
                    activeFocusOnTab: true
                    Accessible.name: i18n("History range")
                    onActivated: {
                        historyController.rangeIndex = currentIndex;
                        historyController.refresh();
                    }
                }
                QQC2.ComboBox {
                    id: sourceCombo
                    objectName: "historySourceCombo"
                    visible: !historyController.compareMode
                    Layout.fillWidth: true
                    Layout.columnSpan: 1
                    model: historyController.sourceOptions()
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: historyController.sourceIndex()
                    activeFocusOnTab: true
                    Accessible.name: i18n("History source")
                    onActivated: {
                        historyController.selectedSourceId = currentValue || "";
                        historyController.normalizeSelection();
                        Qt.callLater(historyController.refresh);
                    }
                }
                QQC2.ComboBox {
                    id: metricCombo
                    objectName: "historyMetricCombo"
                    Layout.fillWidth: true
                    Layout.columnSpan: 1
                    model: historyController.metricRows()
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: historyController.metricIndex()
                    enabled: count > 0
                    activeFocusOnTab: true
                    Accessible.name: i18n("History metric")
                    onActivated: {
                        historyController.selectedMetric = currentValue || "";
                        historyController.refresh();
                    }
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: historyController.metricRows().length === 0
                    text: i18n("No metrics retained")
                    color: Kirigami.Theme.disabledTextColor
                    Accessible.name: text
                }
                PlasmaComponents.Button {
                    objectName: "historyExportButton"
                    Layout.fillWidth: true
                    text: i18n("Export file")
                    icon.name: "document-export"
                    activeFocusOnTab: true
                    Accessible.name: i18n("Export history to a file")
                    enabled: !historyController.loading
                        && historyController.seriesData.length > 0
                    onClicked: exportMenu.open()

                    QQC2.Menu {
                        id: exportMenu
                        y: parent.height
                        QQC2.MenuItem {
                            text: i18n("Export CSV file")
                            onTriggered: history.openExport("csv")
                        }
                        QQC2.MenuItem {
                            text: i18n("Export JSON file")
                            onTriggered: history.openExport("json")
                        }
                    }
                }
                PlasmaComponents.ToolButton {
                    objectName: "historyCopyCsvButton"
                    Layout.fillWidth: true
                    text: i18n("Copy CSV")
                    display: QQC2.AbstractButton.TextBesideIcon
                    icon.name: "edit-copy"
                    enabled: !historyController.loading
                        && historyController.seriesData.length > 0
                    activeFocusOnTab: true
                    Accessible.name: i18n("Copy history as CSV")
                    onClicked: clipboard.setText(historyController.exportPayload("csv"))
                }
            }

            PlasmaComponents.BusyIndicator {
                objectName: "historyLoadingIndicator"
                Layout.alignment: Qt.AlignHCenter
                visible: !history.sessionsMode
                    && (historyController.catalogLoading || historyController.loading)
                running: visible
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                visible: !history.sessionsMode && history.exportStatus !== ""
                text: history.exportStatus
                color: Kirigami.Theme.neutralTextColor
                wrapMode: Text.WordWrap
                Accessible.name: text
            }

            Components.EmptyState {
                objectName: "historyEmptyState"
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !history.sessionsMode && !historyController.catalogLoading
                    && !historyController.loading
                    && (historyController.errorKey !== ""
                        || historyController.seriesData.length === 0)
                title: history.emptyStateTitle()
                details: history.emptyStateDetails()
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                visible: !history.sessionsMode && !historyController.loading
                    && historyController.seriesData.length > 0
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: historyController.seriesData
                    PlasmaComponents.Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: i18nc("History coverage metadata", "%1: %2",
                                    modelData.name,
                                    historyController.coverageText(modelData))
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.WordWrap
                        Accessible.name: text
                    }
                }
            }

            Monitor.MultiSeriesChart {
                objectName: "historyUsageChart"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Kirigami.Units.gridUnit * 12
                visible: !history.sessionsMode && !historyController.loading && historyController.errorKey === ""
                    && historyController.seriesData.length > 0
                seriesData: historyController.seriesData
                metric: historyController.activeMetric || "cost"
            }

            Components.LocalSessionPanel {
                id: sessionsPanel
                objectName: "historySessionsPanel"
                presentationMode: "history"
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: history.sessionsMode
                sessionModel: history.monitor ? history.monitor.localSessions : null
                initialToolId: history.sessionToolId
                initialProjectId: history.sessionProjectId
                initialRangeDays: history.requestedRangeDays
                sessionState: history.sessionState
                onFiltersChanged: function(toolId, projectId, rangeDays) {
                    history.sessionToolId = toolId;
                    history.sessionProjectId = projectId;
                    history.requestedRangeDays = rangeDays;
                }
            }

        }
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export History")
        fileMode: Dialogs.FileDialog.SaveFile
        onAccepted: {
            history.exportStatus = AppInfo.exportConfig(
                historyController.exportPayload(history.pendingExportFormat),
                selectedFile.toString())
                ? i18n("History export saved.")
                : i18n("The history export could not be written.");
        }
    }

    ClipboardHelper {
        id: clipboard
    }

    Component.onCompleted: Qt.callLater(function() {
        history.refreshHistoryCatalog(false);
    })
}
