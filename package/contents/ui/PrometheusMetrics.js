.pragma library

var knownCurrencies = ("AED AFN ALL AMD AOA ARS AUD AWG AZN BAM BBD BDT BMD "
    + "BND BOB BOV BRL BSD BTN BWP BYN BZD CAD CDF CHE CHF CHW CNY COP COU "
    + "CRC CUP CVE CZK DKK DOP DZD EGP ERN ETB EUR FJD FKP GBP GEL GHS GIP "
    + "GMD GTQ GYD HKD HNL HTG HUF IDR ILS INR IRR JMD KES KGS KHR KPW KYD "
    + "KZT LAK LBP LKR LRD LSL MAD MDL MGA MKD MMK MNT MOP MRU MUR MVR MWK "
    + "MXN MXV MYR MZN NAD NGN NIO NOK NPR NZD PAB PEN PGK PHP PKR PLN QAR "
    + "RON RSD RUB SAR SBD SCR SDG SEK SGD SHP SLE SOS SRD SSP STN SVC SYP "
    + "SZL THB TJS TMT TOP TRY TTD TWD TZS UAH USD USN UYU UZS VED VES WST "
    + "XCD XCG YER ZAR ZMW ZWG BIF CLP DJF GNF ISK JPY KMF KRW PYG RWF UGX "
    + "UYI VND VUV XAF XOF XPF BHD IQD JOD KWD LYD OMR TND CLF UYW")
    .split(" ");

function labelValue(value) {
    return (value || "").toString()
        .replace(/\\/g, "\\\\")
        .replace(/\r\n|\r|\n/g, "\\n")
        .replace(/"/g, "\\\"");
}

function normalizedCurrency(currency) {
    var code = String(currency || "").trim().toUpperCase();
    return knownCurrencies.indexOf(code) >= 0 ? code : "";
}

function isFiniteAmount(value) {
    return typeof value === "number" && Number.isFinite(value);
}

function addCurrencyValue(totals, currency, value) {
    var code = normalizedCurrency(currency);
    if (!code || !isFiniteAmount(value)) return false;
    var current = Object.prototype.hasOwnProperty.call(totals, code)
        ? totals[code] : 0;
    if (!isFiniteAmount(current)) return false;
    var total = current + value;
    if (!Number.isFinite(total)) return false;
    totals[code] = total;
    return true;
}

function appendCurrencyMetrics(lines, metric, extraLabels, totals) {
    var currencies = Object.keys(totals || {}).sort();
    for (var i = 0; i < currencies.length; i++) {
        var code = normalizedCurrency(currencies[i]);
        var amount = totals[currencies[i]];
        if (!code || !isFiniteAmount(amount)) continue;
        var prefix = extraLabels ? extraLabels + "," : "";
        lines.push(metric + "{" + prefix + "currency=\"" + code + "\"} "
                   + amount);
    }
}

function appendCostClassMetrics(lines, costs) {
    costs = costs || {};
    appendCurrencyMetrics(lines, "ai_usage_api_spend", "period=\"current\"",
                          costs.actualCurrent || {});
    appendCurrencyMetrics(lines, "ai_usage_api_spend", "period=\"today\"",
                          costs.actualToday || {});
    appendCurrencyMetrics(lines, "ai_usage_api_spend", "period=\"month\"",
                          costs.actualMonth || {});
    appendCurrencyMetrics(lines, "ai_usage_subscription_fees",
                          "period=\"month\",cost_source=\"self_tracked\"",
                          costs.subscriptionFees || {});
    appendCurrencyMetrics(lines, "ai_usage_estimated_burn",
                          "period=\"month\",cost_source=\"estimated_from_usage\"",
                          costs.estimatedBurn || {});
}

function freshnessMaxAgeMs(scheduledIntervalMs) {
    var interval = typeof scheduledIntervalMs === "number"
        && Number.isFinite(scheduledIntervalMs) && scheduledIntervalMs > 0
        ? scheduledIntervalMs : 0;
    return Math.max(15 * 60 * 1000, interval + 5 * 60 * 1000);
}

function metricIsFresh(metric, nowMs, maxAgeMs) {
    var observedAt = dateMilliseconds(metric && metric.observedAt);
    var currentTime = nowMs === undefined ? Date.now() : nowMs;
    var resetAt = dateMilliseconds(metric && metric.resetAt);
    var freshnessLimit = typeof maxAgeMs === "number"
        && Number.isFinite(maxAgeMs) && maxAgeMs > 0
        ? maxAgeMs : 15 * 60 * 1000;
    return !!(metric && metric.available === true
        && observedAt > 0 && observedAt <= currentTime
        && currentTime - observedAt < freshnessLimit
        && (!resetAt || resetAt > currentTime));
}

function addProviderCostMetric(costs, metric, providerEstimated, estimatedMonthlyCost,
                               nowMs, maxAgeMs) {
    if (!metric || metric.kind !== "cost"
            || metric.aggregationLevel === "scoped"
            || metric.modelScope || metric.projectScope
            || String(metric.scope || "").startsWith("organization_scoped")
            || !metricIsFresh(metric, nowMs, maxAgeMs)
            || !isFiniteAmount(metric.value)
            || !normalizedCurrency(metric.currency)) {
        return false;
    }

    var actualCost = ["billing_api", "usage_api", "actual_api", "metrics_api"]
        .indexOf(metric.source) >= 0;
    if (actualCost) {
        if (metric.window === "current")
            return addCurrencyValue(costs.actualCurrent, metric.currency, metric.value);
        if (metric.window === "day") {
            // Period-bounded rows are individual billing periods, not the
            // provider's canonical daily total.
            if (dateMilliseconds(metric.periodStart) > 0
                    || dateMilliseconds(metric.periodEnd) > 0)
                return false;
            return addCurrencyValue(costs.actualToday, metric.currency, metric.value);
        }
        if (metric.window === "month")
            return addCurrencyValue(costs.actualMonth, metric.currency, metric.value);
        return false;
    }

    if (["estimated_pricing", "estimated_from_usage"].indexOf(metric.source) >= 0
            && metric.window === "current" && providerEstimated
            && isFiniteAmount(estimatedMonthlyCost)) {
        return addCurrencyValue(costs.estimatedBurn, metric.currency,
                                estimatedMonthlyCost);
    }
    return false;
}

function dateMilliseconds(value) {
    if (value === undefined || value === null || value === "") return 0;
    var milliseconds = value instanceof Date ? value.getTime() : Date.parse(value);
    return Number.isFinite(milliseconds) ? milliseconds : 0;
}

function appendLocalActivityMetrics(lines, toolKey, usageCount, usageLimit,
                                    lastActivity, nowMs) {
    var currentTime = nowMs === undefined ? Date.now() : nowMs;
    var observedAt = dateMilliseconds(lastActivity);
    var activityFresh = observedAt > 0 && observedAt <= currentTime
        && currentTime - observedAt < 15 * 60 * 1000;
    var hasCount = isFiniteAmount(usageCount);
    var hasLimit = isFiniteAmount(usageLimit) && usageLimit > 0;

    if (activityFresh && hasCount) {
        lines.push("ai_usage_tool_usage_count{tool=\"" + labelValue(toolKey)
                   + "\"} " + usageCount);
        if (hasLimit) {
            lines.push("ai_usage_tool_percent_used{tool=\"" + labelValue(toolKey)
                       + "\"} " + (usageCount * 100 / usageLimit));
        }
    }
    if (hasLimit) {
        lines.push("ai_usage_tool_usage_limit{tool=\"" + labelValue(toolKey)
                   + "\"} " + usageLimit);
    }
    if (observedAt > 0) {
        lines.push("ai_usage_tool_last_activity_seconds{tool=\""
                   + labelValue(toolKey) + "\"} " + observedAt / 1000);
    }
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
