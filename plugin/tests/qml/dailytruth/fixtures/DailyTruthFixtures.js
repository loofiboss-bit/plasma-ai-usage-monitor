.pragma library

function metric(kind, value, available, options) {
    var result = options || {};
    result.kind = kind;
    result.value = value;
    result.available = available;
    return result;
}

function provider(id, metrics) {
    return {
        configKey: id,
        name: id,
        enabled: true,
        monitoringLevel: "actual_usage_spend",
        backend: {
            connected: true,
            error: "",
            usageSource: "actual_api",
            costSource: "billing_api",
            currency: "USD",
            metrics: metrics || []
        }
    };
}

function tool(id, monitor) {
    return {
        stableId: id,
        name: id,
        enabled: true,
        monitor: monitor || { installed: true, quotaWindows: [] }
    };
}

function source(id, kind, readiness, level) {
    return {
        stableId: id,
        displayName: id,
        sourceKindKey: kind,
        monitoringLevel: level || (kind === "provider" ? "actual_usage_spend" : "local_estimate"),
        readinessStateKey: readiness,
        nextActionKey: readiness === "ready_to_verify" ? "verify_source" : "none",
        nextActionText: "Fixture action",
        errorCode: ""
    };
}

function providerOnly() {
    return {
        providers: [provider("openai", [metric("input_tokens", 10, true)])],
        tools: [],
        sources: { openai: source("openai", "provider", "reporting_actual") }
    };
}

function toolOnly() {
    return {
        providers: [],
        tools: [tool("codex", { installed: true, usageLimit: 100, percentUsed: 25, quotaWindows: [] })],
        sources: { codex: source("codex", "local_tool", "reporting_estimate") }
    };
}

function connectivityOnly() {
    return {
        providers: [provider("anthropic", [])],
        tools: [],
        sources: {
            anthropic: source("anthropic", "provider", "connected_connectivity_only", "connectivity_only")
        }
    };
}

function needsConfiguration() {
    return {
        providers: [provider("openai", [])],
        tools: [],
        sources: { openai: source("openai", "provider", "needs_configuration") }
    };
}

function mixed() {
    var providerFixture = providerOnly();
    var toolFixture = toolOnly();
    return {
        providers: providerFixture.providers,
        tools: toolFixture.tools,
        sources: {
            openai: providerFixture.sources.openai,
            codex: toolFixture.sources.codex
        }
    };
}

function empty() {
    return { providers: [], tools: [], sources: {} };
}
