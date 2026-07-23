import QtQuick

QtObject {
    id: state

    property var configuredProviders: []
    property var configuredTools: []
    property var storedCatalog: []

    readonly property var sourceRows: buildSourceRows()
    readonly property var availableMetricKinds: buildAvailableMetricKinds()

    function identity(sourceKind, dbName) {
        return sourceKind + ":" + dbName;
    }

    function configuredRows() {
        var rows = [];
        var providers = configuredProviders || [];
        var tools = configuredTools || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i] || {};
            if (!provider.dbName) continue;
            rows.push({
                historyId: identity("provider", provider.dbName),
                stableId: provider.configKey || provider.dbName,
                dbName: provider.dbName,
                displayName: provider.name || provider.label || provider.dbName,
                sourceKind: "provider",
                enabled: !!provider.enabled,
                color: provider.color || "",
                iconSource: provider.iconSource || ""
            });
        }
        for (var j = 0; j < tools.length; j++) {
            var tool = tools[j] || {};
            if (!tool.name) continue;
            rows.push({
                historyId: identity("tool", tool.name),
                stableId: tool.stableId || tool.name,
                dbName: tool.name,
                displayName: tool.name,
                sourceKind: "tool",
                enabled: !!tool.enabled,
                color: tool.monitor?.toolColor || "",
                iconSource: tool.iconSource || ""
            });
        }
        return rows;
    }

    function buildSourceRows() {
        var byIdentity = {};
        var order = [];
        var configured = configuredRows();
        var stored = storedCatalog || [];

        for (var i = 0; i < configured.length; i++) {
            var configuredRow = configured[i];
            byIdentity[configuredRow.historyId] = configuredRow;
            order.push(configuredRow.historyId);
        }

        for (var j = 0; j < stored.length; j++) {
            var storedRow = stored[j] || {};
            var key = storedRow.historyId
                || identity(storedRow.sourceKind, storedRow.dbName);
            if (!key || !storedRow.dbName) continue;
            if (!byIdentity[key]) {
                byIdentity[key] = {
                    historyId: key,
                    stableId: key,
                    dbName: storedRow.dbName,
                    displayName: storedRow.displayName || storedRow.dbName,
                    sourceKind: storedRow.sourceKind,
                    enabled: false,
                    color: "",
                    iconSource: ""
                };
                order.push(key);
            }
            var row = byIdentity[key];
            row.metricKinds = storedRow.metricKinds || [];
            row.sampleCount = Number(storedRow.sampleCount || 0);
            row.firstObservation = storedRow.firstObservation;
            row.lastObservation = storedRow.lastObservation;
            row.hasHistory = row.sampleCount > 0;
        }

        var result = [];
        for (var k = 0; k < order.length; k++) {
            var current = byIdentity[order[k]];
            current.metricKinds = current.metricKinds || [];
            current.sampleCount = Number(current.sampleCount || 0);
            current.hasHistory = !!current.hasHistory;
            current.historyOnly = current.hasHistory
                && configured.findIndex(function(candidate) {
                    return candidate.historyId === current.historyId;
                }) < 0;
            current.statusKey = current.historyOnly ? "history_only"
                : current.enabled ? "enabled" : "disabled";
            result.push(current);
        }

        result.sort(function(left, right) {
            if (left.hasHistory !== right.hasHistory)
                return left.hasHistory ? -1 : 1;
            return left.displayName.localeCompare(right.displayName);
        });
        return result;
    }

    function source(historyId) {
        for (var i = 0; i < sourceRows.length; i++) {
            if (sourceRows[i].historyId === historyId) return sourceRows[i];
        }
        return ({});
    }

    function metricsForSource(historyId) {
        return source(historyId).metricKinds || [];
    }

    function buildAvailableMetricKinds() {
        var seen = {};
        var result = [];
        for (var i = 0; i < sourceRows.length; i++) {
            var metrics = sourceRows[i].metricKinds || [];
            for (var j = 0; j < metrics.length; j++) {
                if (seen[metrics[j]]) continue;
                seen[metrics[j]] = true;
                result.push(metrics[j]);
            }
        }
        return result;
    }

    function sourcesForMetric(metricKind) {
        return sourceRows.filter(function(row) {
            return row.hasHistory
                && (row.metricKinds || []).indexOf(metricKind) >= 0;
        });
    }
}
