/**
 * Shared utility functions for AI Usage Monitor QML components.
 */

/**
 * Format a large number with K/M suffix.
 * @param {number} n - The number to format
 * @returns {string} Formatted string (e.g., "1.2M", "5.3K", "42")
 */
function formatNumber(n) {
    if (n >= 1000000) return (n / 1000000).toFixed(1) + "M";
    if (n >= 1000) return (n / 1000).toFixed(1) + "K";
    return Math.round(n).toString();
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
