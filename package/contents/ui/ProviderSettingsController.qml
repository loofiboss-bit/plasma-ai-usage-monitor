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
        if (level === "actual_usage_spend" || level === "actual_key_usage") return "usage";
        if (level === "local_activity_estimate") return "local";
        if (level === "gateway_aggregate") return "gateway";
        if (level === "balance_connectivity") return "balance";
        return "connectivity";
    }

    function categoryLabel(level) {
        var labels = {
            "usage": qsTr("Usage & spend"),
            "local": qsTr("Detected local tools"),
            "gateway": qsTr("Gateway"),
            "balance": qsTr("Balance"),
            "connectivity": qsTr("Connectivity only")
        };
        return labels[categoryKey(level)];
    }

    function categoryRank(level) {
        var ranks = { "usage": 0, "local": 1, "gateway": 2, "balance": 3, "connectivity": 4 };
        return ranks[categoryKey(level)];
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
            if (filterKey !== "all" && filterKey !== category) continue;
            rows.push(Object.assign({}, descriptor, {
                categoryKey: category,
                categoryLabel: categoryLabel(descriptor.monitoringLevel),
                categoryRank: categoryRank(descriptor.monitoringLevel)
            }));
        }
        rows.sort(function(left, right) {
            if (left.categoryRank !== right.categoryRank)
                return left.categoryRank - right.categoryRank;
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
        if (descriptorFor(selectedSourceId).configKey) return;
        var rows = buildVisibleSources();
        selectedSourceId = rows.length > 0 ? rows[0].configKey : "";
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
        configurationEdited();
    }

    function trackedKeys() {
        var keys = ["advancedSettingsMode"];
        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            if (descriptor.enabledConfigKey) keys.push(descriptor.enabledConfigKey);
            if (descriptor.modelConfigKey) keys.push(descriptor.modelConfigKey);
            if (descriptor.customBaseUrlConfigKey) keys.push(descriptor.customBaseUrlConfigKey);
        }
        return keys.concat([
            "openaiProjectId", "azureDeploymentId", "bedrockRegion",
            "googleTier", "googleveoTier"
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

    onDescriptorsChanged: extendBaselineForNewSources()
}
