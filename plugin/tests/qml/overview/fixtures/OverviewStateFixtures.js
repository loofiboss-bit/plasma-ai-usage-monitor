.pragma library

function backend(options) {
    var values = options || {};
    return {
        connected: values.connected !== false,
        error: values.error || "",
        usageSource: values.usageSource || "unknown",
        costSource: values.costSource || "unknown",
        currency: values.currency || "USD",
        cost: values.cost === undefined ? 0 : values.cost,
        metrics: values.metrics || []
    };
}

function provider(id, level, backendValues) {
    return {
        configKey: id,
        name: id,
        enabled: true,
        monitoringLevel: level,
        backend: backend(backendValues)
    };
}

function source(id, level, readiness, action, errorCode) {
    return {
        stableId: id,
        displayName: id,
        sourceKindKey: "provider",
        monitoringLevel: level,
        readinessStateKey: readiness,
        nextActionKey: action || "none",
        nextActionText: action && action !== "none" ? "Fixture recovery action" : "No action required",
        errorCode: errorCode || ""
    };
}

function actual() {
    return {
        providers: [provider("openai", "actual_usage_spend", {
            usageSource: "actual_api", costSource: "billing_api", cost: 0,
            metrics: [{ kind: "cost", available: true, value: 0, currency: "USD" }]
        })],
        sources: { openai: source("openai", "actual_usage_spend", "reporting_actual") }
    };
}

function estimate() {
    return {
        providers: [provider("estimated", "actual_usage_spend", {
            usageSource: "estimated_from_usage", costSource: "estimated_from_usage",
            metrics: [{ kind: "input_tokens", available: true, value: 1200 }]
        })],
        sources: { estimated: source("estimated", "actual_usage_spend", "reporting_estimate") }
    };
}

function balance() {
    return {
        providers: [provider("deepseek", "balance_connectivity", {
            usageSource: "connectivity_probe", costSource: "connectivity_probe",
            metrics: [{ kind: "credit_balance", available: true, value: 0, currency: "USD" }]
        })],
        sources: { deepseek: source("deepseek", "balance_connectivity", "reporting_actual") }
    };
}

function connectivity() {
    return {
        providers: [provider("anthropic", "connectivity_only", {
            usageSource: "connectivity_probe", costSource: "connectivity_probe",
            metrics: [
                { kind: "input_tokens", available: false, value: 0 },
                { kind: "cost", available: false, value: 0 }
            ]
        })],
        sources: { anthropic: source("anthropic", "connectivity_only", "connected_connectivity_only") }
    };
}

function stale() {
    return {
        providers: [provider("openai", "actual_usage_spend", {
            usageSource: "actual_api", costSource: "billing_api",
            metrics: [{ kind: "input_tokens", available: true, value: 42 }]
        })],
        sources: { openai: source("openai", "actual_usage_spend", "degraded", "refresh_stale_data", "stale") }
    };
}

function error() {
    return {
        providers: [provider("openai", "actual_usage_spend", {
            connected: false, error: "Fixture error"
        })],
        sources: { openai: source("openai", "actual_usage_spend", "failed", "replace_credentials", "authentication") }
    };
}

function mixedCurrency() {
    return {
        providers: [
            provider("usd", "actual_usage_spend", {
                costSource: "billing_api", currency: "USD", cost: 10,
                metrics: [{ kind: "cost", available: true, value: 10, currency: "USD" }]
            }),
            provider("eur", "actual_usage_spend", {
                costSource: "billing_api", currency: "EUR", cost: 8,
                metrics: [{ kind: "cost", available: true, value: 8, currency: "EUR" }]
            })
        ],
        sources: {
            usd: source("usd", "actual_usage_spend", "reporting_actual"),
            eur: source("eur", "actual_usage_spend", "reporting_actual")
        }
    };
}

function empty() {
    return { providers: [], sources: {} };
}
