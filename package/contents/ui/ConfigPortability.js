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
    for (var p = 0; p < configData.budgetPolicies.length; ++p) {
        var policyError = validatePolicy(configData.budgetPolicies[p]);
        if (policyError.length > 0)
            return { ok: false, error: "policy " + p + ": " + policyError };
    }
    return { ok: true, error: "", settings: settings,
             budgetPolicies: configData.budgetPolicies };
}
