import QtQuick

QtObject {
    id: state

    property var providers: []
    property var tools: []
    property var readinessModel: null
    property int revision: 0

    readonly property var sourceRows: buildSourceRows()
    readonly property var summary: buildSummary(sourceRows)
    readonly property var reportingProviders: filterProviders(["actual", "estimate", "balance", "degraded"])
    readonly property var connectivityProviders: filterProviders(["connectivity"])
    readonly property var attentionRows: sourceRows.filter(function(row) { return row.needsAttention; })

    property Connections readinessConnections: Connections {
        target: state.readinessModel
        enabled: state.readinessModel !== null

        function onSourceChanged(stableId) {
            state.revision++;
        }
    }

    function sourceSnapshot(stableId) {
        var currentRevision = revision;
        if (!readinessModel || typeof readinessModel.source !== "function") return ({});
        return readinessModel.source(stableId) || ({});
    }

    function providerHasMetric(provider, kinds) {
        var metrics = provider?.backend?.metrics || [];
        for (var i = 0; i < metrics.length; i++) {
            if (kinds.indexOf(metrics[i].kind) >= 0 && metrics[i].available === true)
                return true;
        }
        return false;
    }

    function providerHasUsefulMetrics(provider) {
        return providerHasMetric(provider, [
            "input_tokens", "output_tokens", "requests", "cost", "credit_balance",
            "request_limit", "request_remaining", "token_limit", "token_remaining"
        ]);
    }

    function toolHasQuotaData(tool) {
        var monitor = tool?.monitor;
        if (!monitor) return false;
        var rows = monitor.quotaWindows || [];
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i] || {};
            if (row.availability === "disabled" || row.availability === "unavailable"
                    || row.precision === "availability_only") continue;
            if (Number.isFinite(Number(row.percentUsed))) return true;
        }
        return Number(monitor.usageLimit) > 0;
    }

    function fallbackSnapshot(item, sourceKindKey) {
        var backend = sourceKindKey === "provider" ? item?.backend : item?.monitor;
        var result = {
            readinessStateKey: "ready_to_verify",
            nextActionKey: "verify_source",
            nextActionText: i18n("Run the safe read-only verification"),
            monitoringLevel: item?.monitoringLevel || "",
            sourceKindKey: sourceKindKey,
            errorCode: ""
        };
        if (!backend) {
            result.readinessStateKey = "failed";
            result.nextActionKey = "retry_later";
            result.nextActionText = i18n("Retry the verification later");
        } else if (backend.error) {
            result.readinessStateKey = "failed";
            result.nextActionKey = "complete_configuration";
            result.nextActionText = i18n("Review this source configuration");
        } else if (sourceKindKey === "local_tool") {
            if (!backend.installed) {
                result.readinessStateKey = "unavailable_locally";
                result.nextActionKey = "install_local_source";
                result.nextActionText = i18n("Install the local tool, then check again");
            } else if (toolHasQuotaData(item)) {
                result.readinessStateKey = "reporting_estimate";
                result.nextActionKey = "none";
                result.nextActionText = i18n("No action required");
            }
        } else if (backend.connected) {
            var source = backend.usageSource || backend.costSource || "";
            if (source === "connectivity_probe" || source === "connectivity_read_only"
                    || source === "model_discovery_api") {
                result.readinessStateKey = "connected_connectivity_only";
                result.nextActionKey = "none";
                result.nextActionText = i18n("No action required");
            } else if (providerHasUsefulMetrics(item)) {
                result.readinessStateKey = source === "estimated_from_usage"
                    ? "reporting_estimate" : "reporting_actual";
                result.nextActionKey = "none";
                result.nextActionText = i18n("No action required");
            }
        }
        return result;
    }

    function qualityClass(item, snapshot, sourceKindKey) {
        var readiness = snapshot.readinessStateKey || "ready_to_verify";
        if (readiness === "reporting_actual") {
            if ((snapshot.monitoringLevel || item?.monitoringLevel) === "balance_connectivity")
                return "balance";
            return "actual";
        }
        if (readiness === "reporting_estimate") return "estimate";
        if (readiness === "connected_connectivity_only") return "connectivity";
        if (readiness === "degraded" && sourceKindKey === "provider" && providerHasUsefulMetrics(item))
            return "degraded";
        return "attention";
    }

    function rowFor(item, stableId, displayName, sourceKindKey) {
        var snapshot = sourceSnapshot(stableId);
        if (!snapshot.readinessStateKey)
            snapshot = fallbackSnapshot(item, sourceKindKey);
        var quality = qualityClass(item, snapshot, sourceKindKey);
        var readiness = snapshot.readinessStateKey || "ready_to_verify";
        return {
            stableId: stableId,
            displayName: snapshot.displayName || displayName,
            sourceKindKey: snapshot.sourceKindKey || sourceKindKey,
            monitoringLevel: snapshot.monitoringLevel || item?.monitoringLevel || "",
            readinessStateKey: readiness,
            nextActionKey: snapshot.nextActionKey || "none",
            nextActionText: snapshot.nextActionText || "",
            errorCode: snapshot.errorCode || "",
            lastVerified: snapshot.lastVerified,
            qualityClass: quality,
            needsAttention: ["needs_configuration", "ready_to_verify", "degraded", "failed", "unavailable_locally"].indexOf(readiness) >= 0,
            item: item
        };
    }

    function buildSourceRows() {
        var currentRevision = revision;
        var rows = [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (!provider.enabled) continue;
            rows.push(rowFor(provider, provider.configKey, provider.name, "provider"));
        }
        for (var j = 0; j < tools.length; j++) {
            var tool = tools[j];
            if (!tool.enabled) continue;
            rows.push(rowFor(tool, tool.stableId || "", tool.name, "local_tool"));
        }
        return rows;
    }

    function buildSummary(rows) {
        var result = {
            enabled: rows.length,
            actual: 0,
            estimate: 0,
            balance: 0,
            connectivity: 0,
            attention: 0,
            verifying: 0,
            useful: 0,
            verified: 0
        };
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            if (row.readinessStateKey === "verifying") result.verifying++;
            if (row.qualityClass === "actual") result.actual++;
            else if (row.qualityClass === "estimate") result.estimate++;
            else if (row.qualityClass === "balance") result.balance++;
            else if (row.qualityClass === "connectivity") result.connectivity++;
            if (row.needsAttention) result.attention++;
        }
        result.useful = result.actual + result.estimate + result.balance;
        result.verified = result.useful + result.connectivity;
        return result;
    }

    function summaryText() {
        var parts = [i18n("%1 of %2 active sources", summary.useful, summary.enabled)];
        if (summary.connectivity > 0)
            parts.push(i18np("%1 connectivity only", "%1 connectivity only", summary.connectivity));
        if (summary.attention > 0)
            parts.push(i18np("%1 needs attention", "%1 need attention", summary.attention));
        return parts.join(i18n(" · "));
    }

    function filterProviders(qualityClasses) {
        var result = [];
        for (var i = 0; i < sourceRows.length; i++) {
            var row = sourceRows[i];
            if (row.sourceKindKey === "provider" && qualityClasses.indexOf(row.qualityClass) >= 0)
                result.push(row.item);
        }
        return result;
    }

    function rowForProvider(provider) {
        for (var i = 0; i < sourceRows.length; i++) {
            if (sourceRows[i].sourceKindKey === "provider"
                    && sourceRows[i].stableId === provider.configKey)
                return sourceRows[i];
        }
        return rowFor(provider, provider.configKey, provider.name, "provider");
    }
}
