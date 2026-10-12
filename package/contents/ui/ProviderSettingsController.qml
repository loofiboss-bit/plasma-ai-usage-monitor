import QtQuick

QtObject {
    id: controller

    required property var descriptors
    required property var configuration

    property string searchText: ""
    property string filterKey: "all"
    property string selectedSourceId: ""
    property var baseline: ({})
    property bool dirty: false

    readonly property var visibleSources: buildVisibleSources()
    readonly property var selectedSource: descriptorFor(selectedSourceId)

    signal configurationEdited()

    function categoryKey(level) {
        if (level === "actual_quota" || level === "local_activity_estimate") return "tools";
        if (level === "actual_usage_spend" || level === "actual_key_usage") return "usage";
        if (level === "gateway_aggregate") return "gateway";
        if (level === "balance_connectivity") return "balance";
        return "connectivity";
    }

    function categoryLabel(level) {
        if (level === "actual_quota" || level === "local_activity_estimate") return qsTr("AI tools");
        var labels = {
            "usage": qsTr("API services"),
            "local": qsTr("API services"),
            "gateway": qsTr("API services"),
            "balance": qsTr("API services"),
            "connectivity": qsTr("API services")
        };
        return labels[categoryKey(level)];
    }

    function sourceGroupKey(source) {
        return source.sourceKind === "ai_tool" || source.sourceKind === "local_tool"
            ? "tools" : "api";
    }

    function sourceGroupLabel(source) {
        return sourceGroupKey(source) === "tools" ? qsTr("AI tools") : qsTr("API services");
    }

    function sourceGroupRank(source) {
        return sourceGroupKey(source) === "tools" ? 0 : 1;
    }

    function buildVisibleSources() {
        var needle = searchText.trim().toLowerCase();
        var rows = [];
        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            var category = categoryKey(descriptor.monitoringLevel);
            var haystack = (descriptor.name + " " + descriptor.configKey + " "
                            + categoryLabel(descriptor.monitoringLevel)).toLowerCase();
            if (needle.length > 0 && haystack.indexOf(needle) < 0) continue;
            if (filterKey === "enabled" && !configuration[configProperty(descriptor.enabledConfigKey)]) continue;
            if (filterKey === "detected"
                    && (descriptor.sourceKind !== "ai_tool" || !descriptor.installed)) continue;
            if (["usage", "tools", "local", "gateway", "balance", "connectivity"].indexOf(filterKey) >= 0
                    && filterKey !== category) continue;
            rows.push(Object.assign({}, descriptor, {
                categoryKey: category,
                categoryLabel: sourceGroupLabel(descriptor),
                sourceGroupRank: sourceGroupRank(descriptor)
            }));
        }
        rows.sort(function(left, right) {
            if (left.sourceGroupRank !== right.sourceGroupRank)
                return left.sourceGroupRank - right.sourceGroupRank;
            var leftEnabled = !!configuration[configProperty(left.enabledConfigKey)];
            var rightEnabled = !!configuration[configProperty(right.enabledConfigKey)];
            if (leftEnabled !== rightEnabled) return leftEnabled ? -1 : 1;
            var leftDetected = sourceGroupKey(left) === "tools" && !!left.installed;
            var rightDetected = sourceGroupKey(right) === "tools" && !!right.installed;
            if (leftDetected !== rightDetected) return leftDetected ? -1 : 1;
            return left.name.localeCompare(right.name);
        });
        return rows;
    }

    function descriptorFor(stableId) {
        for (var i = 0; i < descriptors.length; ++i) {
            if (descriptors[i].configKey === stableId) return descriptors[i];
        }
        return ({});
    }

    function ensureSelection() {
        var rows = buildVisibleSources();
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].configKey === selectedSourceId) return;
        }
        selectedSourceId = rows.length > 0 ? rows[0].configKey : "";
    }

    onSearchTextChanged: ensureSelection()
    onFilterKeyChanged: ensureSelection()
    onDescriptorsChanged: {
        extendBaselineForNewSources();
        ensureSelection();
    }

    function configProperty(configKey) {
        return "cfg_" + configKey;
    }

    function value(configKey) {
        return configuration[configProperty(configKey)];
    }

    function setValue(configKey, newValue) {
        var propertyName = configProperty(configKey);
        if (configuration[propertyName] === newValue) return;
        configuration[propertyName] = newValue;
        updateDirty();
        ensureSelection();
        configurationEdited();
    }

    function trackedKeys() {
        var keys = [];
        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            if (descriptor.enabledConfigKey) keys.push(descriptor.enabledConfigKey);
            if (descriptor.modelConfigKey) keys.push(descriptor.modelConfigKey);
            if (descriptor.legacyPlanConfigKey) keys.push(descriptor.legacyPlanConfigKey);
            if (descriptor.customBaseUrlConfigKey) keys.push(descriptor.customBaseUrlConfigKey);
            if (descriptor.customLimitConfigKey) keys.push(descriptor.customLimitConfigKey);
            if (descriptor.notificationsConfigKey) keys.push(descriptor.notificationsConfigKey);
            if (descriptor.billingModeConfigKey) keys.push(descriptor.billingModeConfigKey);
            if (descriptor.refreshIntervalConfigKey) keys.push(descriptor.refreshIntervalConfigKey);
            if (descriptor.resetDayConfigKey) keys.push(descriptor.resetDayConfigKey);
            if (descriptor.orgNameConfigKey) keys.push(descriptor.orgNameConfigKey);
        }
        return keys.concat([
            "openaiProjectId", "azureDeploymentId", "bedrockRegion",
            "googleTier", "googleveoTier", "browserSyncEnabled",
            "browserSyncBrowser", "browserSyncInterval", "browserSyncProfile"
        ]);
    }

    function takeSnapshot() {
        var next = {};
        var keys = trackedKeys();
        for (var i = 0; i < keys.length; ++i) next[keys[i]] = value(keys[i]);
        baseline = next;
        dirty = false;
    }

    function extendBaselineForNewSources() {
        var next = Object.assign({}, baseline);
        var keys = trackedKeys();
        for (var i = 0; i < keys.length; ++i) {
            if (!Object.prototype.hasOwnProperty.call(next, keys[i]))
                next[keys[i]] = value(keys[i]);
        }
        baseline = next;
        ensureSelection();
        updateDirty();
    }

    function updateDirty() {
        var keys = trackedKeys();
        for (var i = 0; i < keys.length; ++i) {
            if (value(keys[i]) !== baseline[keys[i]]) {
                dirty = true;
                return;
            }
        }
        dirty = false;
    }

    function acceptChanges() {
        takeSnapshot();
    }

    function discardChanges() {
        var keys = trackedKeys();
        for (var i = 0; i < keys.length; ++i)
            configuration[configProperty(keys[i])] = baseline[keys[i]];
        dirty = false;
    }

    Component.onCompleted: {
        ensureSelection();
        takeSnapshot();
    }

}
