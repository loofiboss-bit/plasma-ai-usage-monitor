import QtQuick

QtObject {
    id: controller

    property var usageDb: null
    property var dailyState: null
    property var configuredProviders: []
    property var configuredTools: []

    property bool compareMode: false
    property int rangeIndex: 1
    property string selectedSourceId: ""
    property string selectedMetric: ""

    property bool loading: false
    property int requestGeneration: 0
    property string catalogRequestId: ""
    property string seriesRequestId: ""
    property string activeMetric: ""
    property string errorKey: ""
    property var seriesData: []
    property var querySources: []

    readonly property var sourceState: historyStateObject
    readonly property var sourceRows: historyStateObject.sourceRows

    property Connections databaseConnections: Connections {
        target: controller.usageDb
        enabled: controller.usageDb !== null
        ignoreUnknownSignals: true

        function onHistoryCatalogReady(requestId, sources) {
            controller.acceptCatalog(requestId, sources);
        }

        function onHistorySeriesReady(requestId, payload) {
            controller.acceptSeries(requestId, payload);
        }
    }

    property HistoryState historyStateObject: HistoryState {
        configuredProviders: controller.configuredProviders
        configuredTools: controller.configuredTools
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

    function metricKinds() {
        return compareMode
            ? historyStateObject.availableMetricKinds
            : historyStateObject.metricsForSource(effectiveSourceId());
    }

    function metricRows() {
        return metricKinds().map(function(metric) {
            return { text: metricLabel(metric), value: metric };
        });
    }

    function effectiveSourceId() {
        if (historyStateObject.source(selectedSourceId).historyId) {
            return selectedSourceId;
        }
        return sourceRows.length > 0 ? sourceRows[0].historyId : "";
    }

    function normalizeSelection() {
        selectedSourceId = effectiveSourceId();
        var metrics = metricKinds();
        if (metrics.indexOf(selectedMetric) < 0) {
            selectedMetric = metrics.length > 0 ? metrics[0] : "";
        }
    }

    function rangeDays() {
        return [1, 7, 30, 90][rangeIndex] || 7;
    }

    function bucketMinutes() {
        return [15, 60, 180, 720][rangeIndex] || 60;
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

    function sourceOptions() {
        return sourceRows.map(function(row) {
            return { text: sourceLabel(row), value: row.historyId };
        });
    }

    function sourceIndex() {
        for (var i = 0; i < sourceRows.length; i++) {
            if (sourceRows[i].historyId === selectedSourceId) return i;
        }
        return sourceRows.length > 0 ? 0 : -1;
    }

    function metricIndex() {
        var metrics = metricKinds();
        var index = metrics.indexOf(selectedMetric);
        return index >= 0 ? index : (metrics.length > 0 ? 0 : -1);
    }

    function queryDescriptor(row) {
        var stale = false;
        if (dailyState && typeof dailyState.source === "function"
                && row.stableId) {
            var dailyRow = dailyState.source(row.stableId) || {};
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
        if (!usageDb || typeof usageDb.requestHistoryCatalog !== "function") {
            return;
        }
        catalogRequestId = "catalog-" + (++requestGeneration);
        usageDb.requestHistoryCatalog(catalogRequestId);
    }

    function refresh() {
        if (!usageDb || typeof usageDb.requestHistorySeries !== "function") {
            loading = false;
            return;
        }
        normalizeSelection();
        activeMetric = selectedMetric;
        errorKey = "";
        seriesData = [];
        if (!activeMetric) {
            loading = false;
            return;
        }

        var selectedRows;
        if (compareMode) {
            selectedRows = historyStateObject.sourcesForMetric(activeMetric);
        } else {
            var selected = historyStateObject.source(selectedSourceId);
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
        usageDb.requestHistorySeries(seriesRequestId, querySources, from, now,
                                     activeMetric, bucketMinutes());
    }

    function acceptCatalog(requestId, sources) {
        if (requestId !== catalogRequestId) return;
        historyStateObject.storedCatalog = sources || [];
        normalizeSelection();
        refresh();
    }

    function acceptSeries(requestId, payload) {
        if (requestId !== seriesRequestId) return;
        loading = false;
        var result = payload || {};
        errorKey = result.errorKey || "";
        seriesData = result.ok ? decorateSeries(result.series || []) : [];
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
            var source = historyStateObject.source(row.sourceId) || {};
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
}
