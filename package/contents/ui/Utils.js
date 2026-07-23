/**
 * Shared utility functions for AI Usage Monitor QML components.
 */

/**
 * Format a large number with K/M suffix.
 * @param {number} n - The number to format
 * @returns {string} Formatted string (e.g., "1.2M", "5.3K", "42")
 */
function formatNumber(n, locale) {
    var value = Number(n);
    if (!Number.isFinite(value)) return "";
    var activeLocale = locale || Qt.locale();
    var magnitude = Math.abs(value);
    if (magnitude >= 1000000)
        return (value / 1000000).toLocaleString(activeLocale, "f", 1) + "\u202fM";
    if (magnitude >= 1000)
        return (value / 1000).toLocaleString(activeLocale, "f", 1) + "\u202fK";
    return Math.round(value).toLocaleString(activeLocale, "f", 0);
}

function formatMoney(value, currency, locale) {
    var code = (currency || "USD").toUpperCase();
    var amount = Number(value);
    if (!Number.isFinite(amount)) return "";
    var formatted = amount.toLocaleCurrencyString(locale || Qt.locale(), code);
    return formatted.replace(/ (?!$)/g, "\u00a0");
}

function addCurrencyTotal(totals, currency, value) {
    var code = (currency || "USD").toUpperCase();
    var amount = Number(value);
    if (!Number.isFinite(amount)) return;
    totals[code] = (totals[code] || 0) + amount;
}

function formatCurrencyTotals(totals) {
    var currencies = Object.keys(totals || {}).sort();
    if (currencies.length === 0) return "\u2014";
    var parts = [];
    for (var i = 0; i < currencies.length; i++) {
        var currency = currencies[i];
        parts.push(formatMoney(totals[currency], currency));
    }
    return parts.join(" + ");
}

function availableMetric(backend, kind, window) {
    if (!backend) return null;
    var metrics = backend.metrics || [];
    for (var i = 0; i < metrics.length; i++) {
        var metric = metrics[i] || {};
        if (metric.kind !== kind || metric.available !== true) continue;
        if (window !== undefined && window !== null && (metric.window || "") !== window) continue;
        if (!Number.isFinite(Number(metric.value))) continue;
        return metric;
    }
    return null;
}

function isActualCostSource(source) {
    return ["billing_api", "usage_api", "actual_api"].indexOf(source || "unknown") >= 0;
}

function providerOutcomeBadgeKeys(usageSource, costSource, monitoringLevel, qualityClass) {
    var keys = [];
    function appendUnique(key) {
        if (key !== "" && keys.indexOf(key) < 0) keys.push(key);
    }

    if (usageSource === "actual_api" || usageSource === "usage_api") {
        appendUnique(monitoringLevel === "gateway_aggregate" ? "gateway_usage"
            : monitoringLevel === "actual_key_usage" ? "key_usage" : "provider_usage");
    } else if (usageSource === "estimated_from_usage") appendUnique("estimated_usage");
    else if (usageSource === "self_tracked") appendUnique("self_tracked");
    else if (usageSource === "browser_sync") appendUnique("browser_sync");
    else if (["model_discovery_api", "connectivity_read_only", "connectivity_probe"].indexOf(usageSource) >= 0)
        appendUnique("connectivity_only");

    if (costSource === "billing_api" || costSource === "usage_api" || costSource === "actual_api")
        appendUnique(monitoringLevel === "gateway_aggregate" ? "gateway_spend" : "provider_spend");
    else if (costSource === "estimated_from_usage") appendUnique("estimated_cost");
    else if (costSource === "connectivity_probe") appendUnique("connectivity_only");

    if (keys.length === 0 && qualityClass === "balance") appendUnique("provider_balance");
    if (keys.length === 0 && qualityClass === "connectivity") appendUnique("connectivity_only");
    return keys;
}

function hasCompatibleCostData(backend) {
    if (!backend) return false;
    var metrics = backend.metrics || [];
    var sawCostMetric = false;
    for (var i = 0; i < metrics.length; i++) {
        var metric = metrics[i] || {};
        if (metric.kind !== "cost") continue;
        sawCostMetric = true;
        if (metric.available === true && Number.isFinite(Number(metric.value))) return true;
    }
    if (sawCostMetric) return false;
    return ["billing_api", "usage_api", "actual_api", "estimated_from_usage"]
        .indexOf(backend.costSource || "unknown") >= 0;
}

/**
 * Format a date/time as a human-readable relative time string.
 * @param {Date} dateTime - The date to format
 * @returns {string} Relative time string (e.g., "just now", "5m ago")
 */
function formatRelativeTime(dateTime) {
    if (!dateTime) return "";
    var now = new Date();
    var diff = Math.floor((now - dateTime) / 1000);
    if (diff < 5) return i18n("just now");
    if (diff < 60) return i18n("%1s ago", diff);
    if (diff < 3600) return i18n("%1m ago", Math.floor(diff / 60));
    if (diff < 86400) return i18n("%1h ago", Math.floor(diff / 3600));
    return Qt.formatTime(dateTime, "hh:mm:ss");
}

/**
 * Get a theme-appropriate color for a usage percentage.
 * @param {number} percent - Usage percentage (0-100)
 * @param {object} theme - Kirigami.Theme reference
 * @returns {color} The appropriate status color
 */
function usageColor(percent, theme, warningThreshold, criticalThreshold) {
    var warning = warningThreshold || 80;
    var critical = criticalThreshold || 95;
    if (percent >= critical) return theme.negativeTextColor;
    if (percent >= warning) return theme.neutralTextColor;
    if (percent >= 50) return theme.neutralTextColor;
    return theme.positiveTextColor;
}

/**
 * Get a theme-appropriate color for rate limit remaining ratio.
 * @param {number} remaining - Remaining count
 * @param {number} total - Total limit
 * @param {object} theme - Kirigami.Theme reference
 * @returns {color} The appropriate status color
 */
function rateLimitColor(remaining, total, theme) {
    if (total <= 0) return theme.disabledTextColor;
    var ratio = remaining / total;
    if (ratio > 0.5) return theme.positiveTextColor;
    if (ratio > 0.2) return theme.neutralTextColor;
    return theme.negativeTextColor;
}

/**
 * Get a theme-appropriate color for budget spending ratio.
 * @param {number} spent - Amount spent
 * @param {number} budget - Budget limit
 * @param {object} theme - Kirigami.Theme reference
 * @returns {color} The appropriate status color
 */
function budgetColor(spent, budget, theme, warningThreshold) {
    if (budget <= 0) return theme.disabledTextColor;
    var ratio = spent / budget;
    var warning = (warningThreshold || 80) / 100.0;
    if (ratio < 0.5) return theme.positiveTextColor;
    if (ratio < warning) return theme.neutralTextColor;
    return theme.negativeTextColor;
}
