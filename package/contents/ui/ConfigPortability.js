.pragma library
.import QtQuick 2.15 as QtQuick

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

function validatePolicy(policy) {
    if (!policy || typeof policy !== "object" || Array.isArray(policy))
        return "policy must be an object";
    var requiredStrings = ["policyId", "sourceId", "sourceKind", "scopeMode",
        "valueClass", "currency", "periodType", "timeZoneId"];
    for (var i = 0; i < requiredStrings.length; ++i) {
        var key = requiredStrings[i];
        if (typeof policy[key] !== "string" || policy[key].length === 0)
            return key + " is required";
    }
    if (!/^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/.test(policy.policyId))
        return "policyId must be a UUID";
    if (["aggregate", "scoped"].indexOf(policy.scopeMode) < 0)
        return "invalid scopeMode";
    if (policy.scopeMode === "scoped"
            && (typeof policy.scopeKind !== "string" || policy.scopeKind.length === 0
                || typeof policy.scopeIdentity !== "string" || policy.scopeIdentity.length === 0))
        return "scoped policy identity is required";
    if (["actual", "estimated"].indexOf(policy.valueClass) < 0)
        return "invalid valueClass";
    if (!Number.isSafeInteger(policy.limitMinor) || policy.limitMinor <= 0)
        return "limitMinor must be a positive integer";
    if (!/^[A-Z]{3}$/.test(policy.currency))
        return "invalid currency";
    if (["calendar_day", "iso_week", "calendar_month", "anchored_month",
         "provider_reset"].indexOf(policy.periodType) < 0)
        return "invalid periodType";
    if (policy.periodType === "anchored_month"
            && (!Number.isInteger(policy.anchorDay) || policy.anchorDay < 1 || policy.anchorDay > 28))
        return "invalid anchorDay";
    if (!Number.isInteger(policy.warningPercent)
            || !Number.isInteger(policy.criticalPercent)
            || policy.warningPercent <= 0
            || policy.warningPercent > policy.criticalPercent
            || policy.criticalPercent > 100)
        return "invalid thresholds";
    if (typeof policy.enabled !== "boolean" || typeof policy.notifyEnabled !== "boolean")
        return "enabled and notifyEnabled must be boolean";
    return "";
}

function schemaV3Payload(configData, portableKeys, currentSettings) {
    if (!configData || configData.schemaVersion !== 3
            || !configData.settings || typeof configData.settings !== "object"
            || !Array.isArray(configData.budgetPolicies)) {
        return { ok: false, error: "schema v3 requires settings and budgetPolicies" };
    }

    var validation = settingsPayload(configData.settings, portableKeys, currentSettings || {});
    if (!validation.ok) return validation;
    var allowed = {};
    for (var i = 0; i < portableKeys.length; ++i)
        allowed[portableKeys[i]] = true;
    var settings = {};
    var keys = Object.keys(configData.settings);
    for (var j = 0; j < keys.length; ++j) {
        var key = keys[j];
        if (!allowed[key])
            return { ok: false, error: "unknown setting: " + key };
        var value = configData.settings[key];
        var expected = currentSettings ? currentSettings[key] : undefined;
        if (expected !== undefined && typeof value !== typeof expected)
            return { ok: false, error: "invalid setting type: " + key };
        if (typeof value === "number" && !Number.isFinite(value))
            return { ok: false, error: "invalid setting value: " + key };
        settings[key] = value;
    }
    var policyIds = {};
    for (var p = 0; p < configData.budgetPolicies.length; ++p) {
        var policyError = validatePolicy(configData.budgetPolicies[p]);
        if (policyError.length > 0)
            return { ok: false, error: "invalid-policy" };
        var policyId = configData.budgetPolicies[p].policyId.toLowerCase();
        if (policyIds[policyId]) return { ok: false, error: "duplicate-policy" };
        policyIds[policyId] = true;
    }
    return { ok: true, error: "", settings: settings,
             budgetPolicies: configData.budgetPolicies };
}

function settingValueValid(key, value) {
    var ranges = {
        refreshInterval: [60, 1800], warningThreshold: [50, 95],
        criticalThreshold: [60, 100], budgetWarningPercent: [1, 100],
        notificationCooldownMinutes: [1, 60], webhookCooldownMinutes: [1, 60],
        dndStartHour: [-1, 23], dndEndHour: [-1, 23], updateCheckInterval: [1, 168],
        forecastLeadTimeHours: [1, 168], historyRetentionDays: [7, 365],
        prometheusPort: [1024, 65535], autoExportIntervalMinutes: [5, 1440],
        analystIntensityMode: [0, 1], browserSyncBrowser: [0, 3], copilotResetDay: [1, 28]
    };
    var choices = {
        compactDisplayMode: ["icon", "attention", "lowest-quota", "next-reset", "actual-spend", "active-sources"],
        autoExportFormat: ["both", "json", "csv"],
        copilotBillingMode: ["auto", "premium_requests_legacy", "ai_credits_usage_based"],
        googleTier: ["free", "paid"], googleveoTier: ["free", "paid"]
    };
    if (ranges[key] && (value < ranges[key][0] || value > ranges[key][1])) return false;
    if (choices[key] && choices[key].indexOf(value) < 0) return false;
    if (typeof value === "number") {
        // KConfig Int settings must not overflow or accept negative counters,
        // budgets, plan indexes or intervals. DND alone supports the -1 sentinel.
        if (!Number.isSafeInteger(value) || value > 2147483647) return false;
        if (value < 0 && key !== "dndStartHour" && key !== "dndEndHour") return false;
        if (/RefreshInterval$/.test(key) && key !== "antigravityRefreshInterval"
                && value > 1800) return false;
        if (key === "antigravityRefreshInterval" && value < 60) return false;
    }
    return true;
}

function settingsPayload(settings, portableKeys, currentSettings) {
    if (!settings || typeof settings !== "object" || Array.isArray(settings))
        return { ok: false, error: "invalid-settings" };
    var allowed = {};
    for (var i = 0; i < portableKeys.length; ++i) allowed[portableKeys[i]] = true;
    var keys = Object.keys(settings);
    for (var j = 0; j < keys.length; ++j) {
        var key = keys[j];
        if (!allowed[key]) return { ok: false, error: "unknown-setting" };
        var value = settings[key];
        var expected = currentSettings[key];
        if (value === null || typeof value !== typeof expected
                || (typeof value === "number" && (!Number.isFinite(value)
                    || (Number.isInteger(expected) && !Number.isInteger(value)))))
            return { ok: false, error: "invalid-setting-type" };
        if (!settingValueValid(key, value)) return { ok: false, error: "invalid-setting-value" };
    }
    var warning = settings.warningThreshold === undefined ? currentSettings.warningThreshold : settings.warningThreshold;
    var critical = settings.criticalThreshold === undefined ? currentSettings.criticalThreshold : settings.criticalThreshold;
    if (warning !== undefined && critical !== undefined && warning > critical)
        return { ok: false, error: "invalid-setting-thresholds" };
    return { ok: true, settings: settings };
}

// Library context outlives replaced configuration pages. The window still owns
// each draft instance, so closing Settings releases the unsaved payload.
function createImportDraft(host, objectName) {
    var component = Qt.createComponent("components/ConfigImportDraftStore.qml");
    if (component.status !== QtQuick.Component.Ready) return null;
    return component.createObject(host, { objectName: objectName });
}
