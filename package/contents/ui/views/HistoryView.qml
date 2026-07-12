import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import ".." as Monitor
import "../components" as Components
import "../Utils.js" as Utils

ColumnLayout {
    id: history
    property bool compareMode: false
    property bool loading: false
    property string requestId: ""
    property var snapshots: []
    property var summary: ({})
    property var dailyCosts: []
    property var compareSeries: []
    property date queryFrom: new Date(0)
    property date queryTo: new Date(0)

    readonly property var enabledProviders: (root.allProviders || []).filter(function(p) { return p.enabled; })
    readonly property var enabledTools: (root.allSubscriptionTools || []).filter(function(t) { return t.enabled; })

    function fromDate() {
        var days = rangeCombo.currentIndex === 0 ? 1 : rangeCombo.currentIndex === 1 ? 7 : 30;
        return new Date(Date.now() - days * 86400000);
    }

    function providerNames() {
        return enabledProviders.map(function(p) { return p.dbName; });
    }

    function toolNames() {
        return enabledTools.map(function(t) { return t.name; });
    }

    function refresh() {
        if (!root.usageDb) return;
        queryFrom = fromDate();
        queryTo = new Date();
        requestId = Date.now().toString() + "-" + Math.random().toString();
        loading = true;
        if (compareMode) {
            var source = sourceCombo.currentValue;
            var names = source === "tools" ? toolNames() : providerNames();
            if (names.length === 0) { compareSeries = []; loading = false; return; }
            root.usageDb.requestComparison(requestId, names, queryFrom, queryTo,
                                           source, metricCombo.currentValue,
                                           rangeCombo.currentIndex === 0 ? 15 : rangeCombo.currentIndex === 1 ? 60 : 180);
        } else {
            var provider = providerCombo.currentValue || "";
            if (!provider) { snapshots = []; loading = false; return; }
            root.usageDb.requestHistory(requestId, provider, queryFrom, queryTo);
        }
    }

    function decorateSeries(rows) {
        var result = [];
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            var color = Kirigami.Theme.highlightColor;
            var label = row.name;
            var sourceRows = sourceCombo.currentValue === "tools" ? enabledTools : enabledProviders;
            for (var j = 0; j < sourceRows.length; j++) {
                var key = sourceCombo.currentValue === "tools" ? sourceRows[j].name : sourceRows[j].dbName;
                if (key === row.name) {
                    label = sourceRows[j].name;
                    color = sourceCombo.currentValue === "tools"
                        ? sourceRows[j].monitor?.toolColor || color : sourceRows[j].color;
                    break;
                }
            }
            result.push({ name: label, color: color, points: row.points || [],
                          currency: row.currency || "USD", mixedCurrency: row.mixedCurrency || false,
                          latestValue: row.latestValue || 0, deltaPercent: row.deltaPercent || 0,
                          sampleCount: row.sampleCount || 0 });
        }
        return result;
    }

    function csvEscape(value) {
        var text = String(value ?? "");
        return /[",\r\n]/.test(text) ? "\"" + text.replace(/"/g, "\"\"") + "\"" : text;
    }

    function exportLoaded(format) {
        var payload;
        if (format === "json") {
            payload = JSON.stringify(compareMode ? compareSeries : snapshots, null, 2);
        } else {
            var lines = [];
            if (compareMode) {
                lines.push("name,timestamp,value");
                for (var i = 0; i < compareSeries.length; i++) {
                    for (var p = 0; p < compareSeries[i].points.length; p++) {
                        lines.push([csvEscape(compareSeries[i].name), csvEscape(compareSeries[i].points[p].timestamp),
                                    compareSeries[i].points[p].value].join(","));
                    }
                }
            } else {
                lines.push("timestamp,model,input_tokens,output_tokens,requests,cost,currency,source,quality");
                for (var s = 0; s < snapshots.length; s++) {
                    var row = snapshots[s];
                    lines.push([csvEscape(row.timestamp), csvEscape(row.model), row.inputTokens, row.outputTokens,
                                row.requestCount, row.cost, csvEscape(row.currency), csvEscape(row.costSource),
                                csvEscape(row.dataQuality)].join(","));
                }
            }
            payload = lines.join("\n") + "\n";
        }
        clipboard.setText(payload);
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing
        PlasmaExtras.Heading { level: 4; text: i18n("History"); Layout.fillWidth: true }
        PlasmaComponents.ToolButton { text: compareMode ? i18n("Compare") : i18n("Detail"); onClicked: { compareMode = !compareMode; refresh(); } }
        PlasmaComponents.ToolButton { icon.name: "view-refresh"; enabled: !loading; onClicked: refresh() }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        QQC2.ComboBox { id: rangeCombo; model: [i18n("24 hours"), i18n("7 days"), i18n("30 days")]; currentIndex: 1; onActivated: refresh() }
        QQC2.ComboBox {
            id: providerCombo
            visible: !compareMode
            Layout.fillWidth: true
            model: enabledProviders.map(function(p) { return { text: p.name, value: p.dbName }; })
            textRole: "text"; valueRole: "value"; onActivated: refresh()
        }
        QQC2.ComboBox {
            id: sourceCombo
            visible: compareMode
            model: [{ text: i18n("Providers"), value: "providers" }, { text: i18n("Tools"), value: "tools" }]
            textRole: "text"; valueRole: "value"; onActivated: refresh()
        }
        QQC2.ComboBox {
            id: metricCombo
            visible: compareMode
            model: sourceCombo.currentValue === "tools"
                ? [{ text: i18n("Usage"), value: "usageCount" }, { text: i18n("Percent"), value: "percentUsed" }]
                : [{ text: i18n("Cost"), value: "cost" }, { text: i18n("Tokens"), value: "tokens" }, { text: i18n("Requests"), value: "requests" }]
            textRole: "text"; valueRole: "value"; onActivated: refresh()
        }
        PlasmaComponents.ToolButton { text: "CSV"; enabled: !loading && (compareMode ? compareSeries.length : snapshots.length); onClicked: exportLoaded("csv") }
        PlasmaComponents.ToolButton { text: "JSON"; enabled: !loading && (compareMode ? compareSeries.length : snapshots.length); onClicked: exportLoaded("json") }
    }

    PlasmaComponents.BusyIndicator { Layout.alignment: Qt.AlignHCenter; visible: history.loading; running: visible }

    Components.EmptyState {
        Layout.fillWidth: true; Layout.fillHeight: true
        visible: !loading && (compareMode ? compareSeries.length === 0 : snapshots.length === 0)
        title: i18n("No compatible history data")
        details: i18n("Choose a configured source and time range. Rolling gauges are not presented as calendar-day totals.")
    }

    Monitor.MultiSeriesChart {
        Layout.fillWidth: true; Layout.fillHeight: true
        visible: compareMode && !loading && compareSeries.length > 0
        seriesData: history.compareSeries
        metric: metricCombo.currentValue || "cost"
    }

    ColumnLayout {
        Layout.fillWidth: true; Layout.fillHeight: true
        visible: !compareMode && !loading && snapshots.length > 0
        Monitor.UsageChart {
            Layout.fillWidth: true; Layout.fillHeight: true
            chartData: history.snapshots
            provider: providerCombo.currentText
        }
        Monitor.TrendSummary {
            Layout.fillWidth: true
            summaryData: history.summary
            dailyCosts: history.dailyCosts
            provider: providerCombo.currentText
        }
    }

    ClipboardHelper { id: clipboard }
    Connections {
        target: root.usageDb
        function onHistoryReady(id, payload) {
            if (id !== history.requestId) return;
            history.snapshots = payload.snapshots || [];
            history.summary = payload.summary || ({});
            history.dailyCosts = payload.dailyCosts || [];
            history.loading = false;
        }
        function onComparisonReady(id, rows) {
            if (id !== history.requestId) return;
            history.compareSeries = history.decorateSeries(rows || []);
            history.loading = false;
        }
    }
    Component.onCompleted: refresh()
}
