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
    // Transient handoff state owned by FullRepresentation. Call captureViewState()
    // before replacing this page and assign the returned object on restoration.
    property var viewState: null
    property var pendingViewState: null
    property var catalogDatabase: null
    property bool applyingViewState: false
    property bool restoreScrollPending: false
    property real savedContentY: 0
    signal sessionHistoryRangeRequested(var selection)
    signal sessionDayReturnRequested()
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

    function restoreScrollPosition() {
        Qt.callLater(function() {
            if (!history.restoreScrollPending) return;
            // Controls exposes a Flickable here at runtime, but its qmltypes only declare QQuickItem.
            // qmllint disable missing-property
            var flickable = historyScroll.contentItem;
            if (!flickable || flickable.contentY === undefined) return;
            var maxY = Math.max(0, flickable.contentHeight - flickable.height);
            if (history.savedContentY > 0 && maxY === 0) return;
            flickable.contentY = Math.max(0, Math.min(history.savedContentY, maxY));
            // qmllint enable missing-property
            history.restoreScrollPending = false;
        });
    }

    Connections {
        target: historyScroll.contentItem
        function onContentHeightChanged() {
            if (history.restoreScrollPending) history.restoreScrollPosition();
        }
    }

    function currentScrollPosition() {
        // Controls exposes a Flickable here at runtime, but its qmltypes only declare QQuickItem.
        // qmllint disable missing-property
        var flickable = historyScroll.contentItem;
        return flickable && flickable.contentY !== undefined
            ? Number(flickable.contentY) : 0;
        // qmllint enable missing-property
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

    function selectionSummary() {
        var parts = [];
        if (historyController.compareMode) {
            parts.push(i18n("Compare sources"));
        } else {
            var source = historyController.sourceRows[
                historyController.sourceIndex()];
            if (source) parts.push(source.displayName);
        }
        var metric = historyController.metricLabel(
            historyController.activeMetric || historyController.selectedMetric);
        if (metric) parts.push(metric);
        parts.push(historyController.rangeDays() === 1
            ? i18n("Last 24 hours")
            : i18n("Last %1 days", historyController.rangeDays()));
        return parts.join(" · ");
    }

    function captureViewState() {
        var session = ({
            toolId: sessionsPanel ? sessionsPanel.selectedToolId : sessionToolId,
            projectId: sessionsPanel ? sessionsPanel.selectedProjectId : sessionProjectId,
            modelFilter: sessionsPanel ? sessionsPanel.selectedModelFilter : ({}),
            projectSearch: sessionsPanel ? sessionsPanel.projectSearch : "",
            modelSearch: sessionsPanel ? sessionsPanel.modelSearch : "",
            historyDisplayIndex: sessionsPanel ? sessionsPanel.historyDisplayIndex : 0,
            rangeIndex: sessionsPanel ? sessionsPanel.selectedRangeIndex : 1,
            rangeMode: sessionsPanel && sessionsPanel.selectedRangeIndex === 3 ? "custom" : "preset",
            customStartDate: sessionsPanel ? sessionsPanel.customStartDate : "",
            customEndDate: sessionsPanel ? sessionsPanel.customEndDate : "",
            dayDrilldown: sessionsPanel && sessionsPanel.sessionState
                ? !!sessionsPanel.sessionState.dayDrilldown : false,
            dayFocusDate: sessionsPanel && sessionsPanel.sessionState
                ? String(sessionsPanel.sessionState.dayFocusDate || "") : "",
            dailyScrollY: sessionsPanel && sessionsPanel.sessionState
                ? Number(sessionsPanel.sessionState.dailyScrollY || 0) : 0,
            pageOffset: sessionsPanel ? sessionsPanel.pageOffset : 0,
            sessionId: sessionsPanel ? sessionsPanel.selectedSessionId : ""
        });
        return {
            sessionsMode: history.sessionsMode,
            selectedSourceId: historyController.selectedSourceId,
            selectedMetric: historyController.selectedMetric,
            rangeIndex: historyController.rangeIndex,
            compareMode: historyController.compareMode,
            contentY: currentScrollPosition(),
            sessionToolId: session.toolId,
            sessionProjectId: session.projectId,
            session: session
        };
    }

    function applyViewState(state) {
        if (!state || typeof state !== "object" || Object.keys(state).length === 0)
            return false;
        pendingViewState = state;
        applyingViewState = true;
        sessionsMode = state.sessionsMode === true;
        applyingViewState = false;
        savedContentY = Math.max(0, Number(state.contentY || 0));
        restoreScrollPending = true;
        sessionToolId = String(state.sessionToolId || "codex-cli");
        sessionProjectId = String(state.sessionProjectId || "");

        var session = state.session || ({});
        if (!sessionState || typeof sessionState !== "object")
            sessionState = ({});
        sessionState.toolId = String(session.toolId || sessionToolId);
        sessionState.projectId = String(session.projectId || sessionProjectId);
        sessionState.modelFilter = session.modelFilter || ({});
        sessionState.projectSearch = String(session.projectSearch || "");
        sessionState.modelSearch = String(session.modelSearch || "");
        sessionState.historyDisplayIndex = Number(session.historyDisplayIndex) === 1 ? 1 : 0;
        sessionState.rangeIndex = Math.max(0, Math.min(3,
            Number(session.rangeIndex === undefined ? 1 : session.rangeIndex)));
        sessionState.rangeMode = session.rangeMode === "custom" ? "custom" : "preset";
        sessionState.customStartDate = String(session.customStartDate || "");
        sessionState.customEndDate = String(session.customEndDate || "");
        sessionState.dayDrilldown = session.dayDrilldown === true;
        sessionState.dayFocusDate = String(session.dayFocusDate || "");
        sessionState.dailyScrollY = Math.max(0, Number(session.dailyScrollY || 0));
        sessionState.pageOffset = Math.max(0, Number(session.pageOffset || 0));
        sessionState.sessionId = String(session.sessionId || "");
        sessionsPanel.restoreState();

        var database = history.monitor ? history.monitor.usageDb : null;
        if (!database || history.catalogDatabase !== database
                || historyController.catalogLoading
                || !historyController.sourceRows
                || historyController.sourceRows.length === 0) {
            restoreScrollPosition();
            return false;
        }
        historyController.compareMode = state.compareMode === true;
        historyController.rangeIndex = Math.max(0, Math.min(3,
            Number(state.rangeIndex === undefined ? 1 : state.rangeIndex)));
        if (state.selectedSourceId)
            historyController.selectedSourceId = String(state.selectedSourceId);
        historyController.normalizeSelection();
        if (state.selectedMetric)
            historyController.selectedMetric = String(state.selectedMetric);
        historyController.normalizeSelection();
        pendingViewState = null;
        if (!sessionsMode) Qt.callLater(historyController.refresh);
        restoreScrollPosition();
        return true;
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
        if (viewState && viewState.selectedSourceId) return;
        historyController.applyDeepLink(requestedSourceId, requestedMetric,
                                        requestedRangeDays);
    }

    Connections {
        target: historyController
        function onCatalogAccepted() {
            if (history.pendingViewState)
                history.applyViewState(history.pendingViewState);
            history.applyRequestedSelection();
        }

        function onLoadingChanged() {
            if (!historyController.loading && history.restoreScrollPending)
                history.restoreScrollPosition();
        }
    }

    onMonitorChanged: Qt.callLater(function() {
        history.refreshHistoryCatalog(false);
    })
    onSessionsModeChanged: {
        if (!applyingViewState) scrollToTop();
        if (!sessionsMode)
            catalogRefreshTimer.restart();
    }
    onRequestedSourceIdChanged: Qt.callLater(applyRequestedSelection)
    onRequestedMetricChanged: Qt.callLater(applyRequestedSelection)
    onRequestedRangeDaysChanged: Qt.callLater(applyRequestedSelection)
    onViewStateChanged: {
        if (viewState) Qt.callLater(function() {
            history.applyViewState(history.viewState);
        });
    }

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
                PlasmaComponents.Label {
                    text: i18n("History filters")
                    font.weight: Font.DemiBold
                    Accessible.name: text
                }
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
                columns: history.width < Kirigami.Units.gridUnit * 40 ? 1 : 4
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
                        objectName: "historyExportMenu"
                        y: parent.height
                        QQC2.MenuItem {
                            text: i18n("Export CSV file")
                            onTriggered: history.openExport("csv")
                        }
                        QQC2.MenuItem {
                            text: i18n("Export JSON file")
                            onTriggered: history.openExport("json")
                        }
                        QQC2.MenuItem {
                            objectName: "historyCopyCsvMenuItem"
                            text: i18n("Copy history as CSV")
                            onTriggered: clipboard.setText(
                                historyController.exportPayload("csv"))
                        }
                    }
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

                PlasmaComponents.Label {
                    objectName: "historySelectionContext"
                    Layout.fillWidth: true
                    text: history.selectionSummary()
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                    Accessible.name: i18n("Showing %1", text)
                }

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
                onSessionHistoryRangeRequested: function(selection) {
                    history.sessionHistoryRangeRequested(selection);
                }
                onSessionDayReturnRequested: history.sessionDayReturnRequested()
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
        if (history.viewState)
            history.applyViewState(history.viewState);
        history.refreshHistoryCatalog(false);
    })
}
