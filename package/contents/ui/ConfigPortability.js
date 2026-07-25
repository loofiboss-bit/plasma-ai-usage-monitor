.pragma library

function schemaV2Settings(configData, portableKeys) {
    var filtered = {};
    if (!configData || configData.schemaVersion !== 2 || !configData.settings) {
        return filtered;
    }

    for (var i = 0; i < portableKeys.length; ++i) {
        var key = portableKeys[i];
        if (configData.settings[key] !== undefined) {
            filtered[key] = configData.settings[key];
        }
    }
    return filtered;
}
