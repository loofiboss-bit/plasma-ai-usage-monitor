.pragma library

function labelValue(value) {
    return (value || "").toString()
        .replace(/\\/g, "\\\\")
        .replace(/\r\n|\r|\n/g, "\\n")
        .replace(/"/g, "\\\"");
}

function appendToolQuotaMetrics(lines, toolKey, windows, nowMs) {
    var actualSources = ["billing_api", "usage_api", "actual_api",
                         "metrics_api", "response_headers", "browser_sync",
                         "antigravity_local", "local_daemon_actual"];
    var currentTime = nowMs === undefined ? Date.now() : nowMs;
    for (var i = 0; i < windows.length; i++) {
        var window = windows[i] || {};
        var source = String(window.source || "");
        var observedAt = Date.parse(window.observedAt);
        var resetAt = Date.parse(window.resetAt);
        var remaining = window.percentRemaining;
        if (typeof remaining !== "number" && typeof window.percentUsed === "number")
            remaining = 100 - window.percentUsed;
        if (actualSources.indexOf(source) < 0
                || !Number.isFinite(remaining) || remaining < 0 || remaining > 100
                || !Number.isFinite(observedAt) || observedAt > currentTime
                || currentTime - observedAt >= 15 * 60 * 1000
                || (Number.isFinite(resetAt) && resetAt <= currentTime))
            continue;
        var labels = "tool=\"" + labelValue(toolKey)
            + "\",kind=\"" + labelValue(window.kind)
            + "\",source=\"" + labelValue(source)
            + "\",quality=\"" + labelValue(window.precision || window.sourceClass) + "\"";
        lines.push("ai_usage_tool_quota_percent_remaining{" + labels + "} " + remaining);
        var resetTimestamp = resetAt / 1000;
        if (Number.isFinite(resetTimestamp)) {
            lines.push("ai_usage_tool_quota_reset_timestamp_seconds{" + labels + "} "
                       + resetTimestamp);
        }
    }
}
