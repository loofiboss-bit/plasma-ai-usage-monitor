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

    property bool compareMode: false
    property bool loading: false
    property int requestGeneration: 0
    property string catalogRequestId: ""
    property string seriesRequestId: ""
    property string activeMetric: ""
    property string errorKey: ""
    property var seriesData: []
    property var querySources: []
    property string pendingExportFormat: "csv"
    property string exportStatus: ""

    Components.HistoryState {
        id: historyState
        configuredProviders: root.allProviders || []
        configuredTools: root.allSubscriptionTools || []
    }

    function metricLabel(metric) {
        var labels = {
            cost: i18n("Cost"),
            tokens: i18n("Tokens"),
            requests: i18n("Requests"),
            rateLimitUsed: i18n("Rate-limit utilization"),
            usageCount: i18n("Tool usage"),
            percentUsed: i18n("Percent used"),
            remaining: i18n("Remaining quota")
        };
        return labels[metric] || metric;
    }

    function metricRows() {
        var metrics = compareMode
            ? historyState.availableMetricKinds
            : historyState.metricsForSource(sourceCombo.currentValue || "");
        return metrics.map(function(metric) {
            return { text: metricLabel(metric), value: metric };
        });
    }

    function rangeDays() {
        return [1, 7, 30, 90][rangeCombo.currentIndex] || 7;
    }

    function bucketMinutes() {
        return [15, 60, 180, 720][rangeCombo.currentIndex] || 60;
    }

    function sourceStatusText(row) {
        if (row.statusKey === "history_only") return i18n("History only");
        if (row.statusKey === "disabled") return i18n("Disabled");
        return i18n("Enabled");
    }

    function sourceLabel(row) {
        return i18nc("History source and status", "%1 — %2",
                     row.displayName, sourceStatusText(row));
    }

    function currentMetric() {
        var rows = metricRows();
        var selected = metricCombo.currentValue || "";
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].value === selected) return selected;
        }
        return rows.length > 0 ? rows[0].value : "";
    }

    function queryDescriptor(row) {
        var stale = false;
        if (root.dailyState && typeof root.dailyState.source === "function"
                && row.stableId) {
            var dailyRow = root.dailyState.source(row.stableId) || {};
            stale = dailyRow.freshnessState === "stale";
        }
        return {
            historyId: row.historyId,
            dbName: row.dbName,
            displayName: row.displayName,
            sourceKind: row.sourceKind,
            historyOnly: row.historyOnly,
            stale: stale
        };
    }

    function refreshCatalog() {
        if (!root.usageDb) return;
        catalogRequestId = "catalog-" + (++requestGeneration);
        root.usageDb.requestHistoryCatalog(catalogRequestId);
    }

    function refresh() {
        if (!root.usageDb) return;
        activeMetric = currentMetric();
        errorKey = "";
        seriesData = [];
        if (!activeMetric) {
            loading = false;
            return;
        }

        var selectedRows;
        if (compareMode) {
            selectedRows = historyState.sourcesForMetric(activeMetric);
        } else {
            var selected = historyState.source(sourceCombo.currentValue || "");
            selectedRows = selected.historyId ? [selected] : [];
        }
        if (selectedRows.length === 0) {
            loading = false;
            return;
        }

        querySources = selectedRows.map(queryDescriptor);
        var now = new Date();
        var from = new Date(now.getTime() - rangeDays() * 86400000);
        seriesRequestId = "series-" + (++requestGeneration);
        loading = true;
        root.usageDb.requestHistorySeries(seriesRequestId, querySources, from, now,
                                          activeMetric, bucketMinutes());
    }

    function decorateSeries(rows) {
        var decorated = [];
        var displayCounts = {};
        for (var c = 0; c < rows.length; c++) {
            var displayName = rows[c].displayName || rows[c].dbName;
            displayCounts[displayName] = Number(displayCounts[displayName] || 0) + 1;
        }
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i] || {};
            var source = historyState.source(row.sourceId) || {};
            var suffixes = [];
            if (row.currency) suffixes.push(row.currency);
            if (row.window && row.window !== "current") suffixes.push(row.window);
            if (row.semantic === "local_estimate") suffixes.push(i18n("estimated"));
            var label = row.displayName || row.dbName;
            if (displayCounts[label] > 1 && row.source)
                suffixes.push(row.source);
            if (displayCounts[label] > 1 && row.scope && row.scope !== "api_key")
                suffixes.push(row.scope);
            if (suffixes.length > 0)
                label += i18nc("History series qualifiers", " (%1)", suffixes.join(", "));
            row.name = label;
            row.color = source.color || "";
            decorated.push(row);
        }
        return decorated;
    }

    function errorText() {
        var messages = {
            mixed_currencies: i18n("These sources use different currencies. Select one source or compare sources with the same currency."),
            incompatible_units: i18n("These sources do not use compatible units."),
            incompatible_semantics: i18n("These sources use different measurement semantics and cannot be compared truthfully."),
            invalid_request: i18n("The selected history request is not valid.")
        };
        return messages[errorKey] || "";
    }

    function coverageText(row) {
        var available = Number(row.availablePointCount || 0);
        var samples = Number(row.sampleCount || 0);
        var text = i18np("%1 plotted point from %2 stored sample",
                         "%1 plotted points from %2 stored samples",
                         available, samples);
        if (row.containsGaps) text += i18n(" · contains gaps");
        if (row.historyOnly) text += i18n(" · history only");
        if (row.stale) text += i18n(" · stale");
        return text;
    }

    function csvEscape(value) {
        var text = String(value ?? "");
        return /[",\r\n]/.test(text)
            ? "\"" + text.replace(/"/g, "\"\"") + "\"" : text;
    }

    function exportPayload(format) {
        if (format === "json") return JSON.stringify(seriesData, null, 2);
        var lines = ["source,kind,metric,unit,currency,semantic,timestamp,value,available"];
        for (var i = 0; i < seriesData.length; i++) {
            var series = seriesData[i];
            var points = series.points || [];
            for (var p = 0; p < points.length; p++) {
                var point = points[p];
                lines.push([
                    csvEscape(series.dbName), csvEscape(series.sourceKind),
                    csvEscape(series.metricKind), csvEscape(series.unit),
                    csvEscape(series.currency), csvEscape(series.semantic),
                    csvEscape(point.timestamp),
                    point.available === false ? "" : point.value,
                    point.available === false ? "false" : "true"
                ].join(","));
            }
        }
        return lines.join("\r\n") + "\r\n";
    }

    function openExport(format) {
        pendingExportFormat = format;
        exportDialog.nameFilters = format === "json"
            ? [i18n("JSON files (*.json)")] : [i18n("CSV files (*.csv)")];
        exportDialog.currentFile = "ai-usage-history." + format;
        exportDialog.open();
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            level: 4
            text: i18n("History")
            Layout.fillWidth: true
        }
        PlasmaComponents.ToolButton {
            text: history.compareMode ? i18n("Compare") : i18n("Detail")
            checkable: true
            checked: history.compareMode
            onClicked: {
                history.compareMode = !history.compareMode;
                Qt.callLater(history.refresh);
            }
        }
        PlasmaComponents.ToolButton {
            icon.name: "view-refresh"
            enabled: !history.loading
            Accessible.name: i18n("Refresh history")
            onClicked: history.refreshCatalog()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        QQC2.ComboBox {
            id: rangeCombo
            model: [i18n("24 hours"), i18n("7 days"), i18n("30 days"), i18n("90 days")]
            currentIndex: 1
            Accessible.name: i18n("History range")
            onActivated: history.refresh()
        }
        QQC2.ComboBox {
            id: sourceCombo
            visible: !history.compareMode
            Layout.fillWidth: true
            model: historyState.sourceRows.map(function(row) {
                return { text: history.sourceLabel(row), value: row.historyId };
            })
            textRole: "text"
            valueRole: "value"
            Accessible.name: i18n("History source")
            onActivated: Qt.callLater(history.refresh)
        }
        QQC2.ComboBox {
            id: metricCombo
            Layout.fillWidth: history.compareMode
            model: history.metricRows()
            textRole: "text"
            valueRole: "value"
            Accessible.name: i18n("History metric")
            onActivated: history.refresh()
        }
        PlasmaComponents.Button {
            text: i18n("Export file")
            icon.name: "document-export"
            enabled: !history.loading && history.seriesData.length > 0
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
            icon.name: "edit-copy"
            enabled: !history.loading && history.seriesData.length > 0
            Accessible.name: i18n("Copy history as CSV")
            onClicked: clipboard.setText(history.exportPayload("csv"))
        }
    }

    PlasmaComponents.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        visible: history.loading
        running: visible
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        visible: history.exportStatus !== ""
        text: history.exportStatus
        color: Kirigami.Theme.neutralTextColor
        wrapMode: Text.WordWrap
        Accessible.name: text
    }

    Components.EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !history.loading
            && (history.errorKey !== "" || history.seriesData.length === 0)
        title: history.errorKey !== ""
            ? i18n("This comparison is not compatible")
            : i18n("No compatible history data")
        details: history.errorKey !== ""
            ? history.errorText()
            : i18n("Choose a source, metric, and time range with retained compatible observations.")
    }

    Monitor.MultiSeriesChart {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !history.loading && history.errorKey === ""
            && history.seriesData.length > 0
        seriesData: history.seriesData
        metric: history.activeMetric || "cost"
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        visible: !history.loading && history.seriesData.length > 0
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: history.seriesData
            PlasmaComponents.Label {
                required property var modelData
                Layout.fillWidth: true
                text: i18nc("History coverage metadata", "%1: %2",
                            modelData.name, history.coverageText(modelData))
                color: Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                Accessible.name: text
            }
        }
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export History")
        fileMode: Dialogs.FileDialog.SaveFile
        onAccepted: {
            history.exportStatus = AppInfo.exportConfig(
                history.exportPayload(history.pendingExportFormat),
                selectedFile.toString())
                ? i18n("History export saved.")
                : i18n("The history export could not be written.");
        }
    }

    ClipboardHelper {
        id: clipboard
    }

    Connections {
        target: root.usageDb

        function onHistoryCatalogReady(id, sources) {
            if (id !== history.catalogRequestId) return;
            historyState.storedCatalog = sources || [];
            Qt.callLater(history.refresh);
        }

        function onHistorySeriesReady(id, payload) {
            if (id !== history.seriesRequestId) return;
            history.loading = false;
            history.errorKey = payload.errorKey || "";
            history.seriesData = payload.ok
                ? history.decorateSeries(payload.series || []) : [];
        }
    }

    Component.onCompleted: refreshCatalog()
}
