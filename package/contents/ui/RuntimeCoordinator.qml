import QtQuick

Item {
    id: runtime

    visible: false
    width: 0
    height: 0

    required property var configuration
    required property var registry
    required property var secrets
    required property var usageDatabase
    required property var notificationController
    required property var scheduler
    required property var metricsServer
    required property var webhookNotifier
    required property var claudeCodeMonitor
    required property var codexCliMonitor
    required property var copilotMonitor
    required property var cursorMonitor
    required property var windsurfMonitor
    required property var jetbrainsAiMonitor

    function loadIntegrationSecrets() {
        if (registry.demoMode) {
            if (copilotMonitor) copilotMonitor.githubToken = "";
            if (registry.bedrockBackend) {
                registry.bedrockBackend.secretAccessKey = "";
                registry.bedrockBackend.sessionToken = "";
            }
            webhookNotifier.slackWebhookUrl = "";
            webhookNotifier.discordWebhookUrl = "";
            return;
        }

        if (copilotMonitor) {
            copilotMonitor.githubToken = secrets.getKey("copilot_github");
        }

        if (registry.bedrockBackend) {
            registry.bedrockBackend.secretAccessKey = secrets.getKey("bedrock_secret_access_key");
            registry.bedrockBackend.sessionToken = secrets.getKey("bedrock_session_token");
        }

        webhookNotifier.slackWebhookUrl = secrets.getKey("slack_webhook_url");
        webhookNotifier.discordWebhookUrl = secrets.getKey("discord_webhook_url");
    }

    function loadProviderApiKey(configKey, reason, shouldRefresh) {
        var provider = registry.providerByConfigKey(configKey);
        if (!provider || !provider.backend) {
            return;
        }
        if (!provider.enabled) {
            provider.backend.cancelRefresh();
            provider.backend.setApiKey("");
            return;
        }
        if (provider.requiresApiKey !== false) {
            var keySlot = provider.secretKey || provider.configKey;
            // Demo mode is deterministic and must never read a real wallet
            // credential. The mock server accepts this non-secret sentinel.
            provider.backend.setApiKey(registry.demoMode ? "demo-key" : secrets.getKey(keySlot));
        }
        if (shouldRefresh) {
            scheduler.refreshProvider(provider, reason, true);
        }
    }

    function loadApiKeys(reason, shouldRefresh) {
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            loadProviderApiKey(provider.configKey,
                               reason === undefined ? scheduler.refreshStartup : reason,
                               shouldRefresh === true);
        }

        loadIntegrationSecrets();
        syncMetricsPayload();
    }

    function recordProviderSnapshot(providerName, backend) {
        if (!usageDatabase.enabled || !backend) {
            return;
        }

        var activeModel = "";
        if (backend.model !== undefined && backend.model !== null) {
            activeModel = backend.model;
        }

        var inputMetric = backend.metric ? backend.metric("input_tokens") : {};
        var outputMetric = backend.metric ? backend.metric("output_tokens") : {};
        var requestsMetric = backend.metric ? backend.metric("requests") : {};
        var costMetric = backend.metric ? backend.metric("cost", "", "current") : {};

        // v13 writes the nullable typed contract for every provider.  The old
        // snapshot table remains a compatibility projection and is updated
        // only when all of its non-nullable core fields are genuinely known.
        if (inputMetric.available && outputMetric.available
                && requestsMetric.available && costMetric.available) {
            usageDatabase.recordSnapshot(
                providerName,
                backend.inputTokens,
                backend.outputTokens,
                backend.requestCount,
                backend.cost,
                backend.dailyCost,
                backend.monthlyCost,
                backend.rateLimitRequests,
                backend.rateLimitRequestsRemaining,
                backend.rateLimitTokens,
                backend.rateLimitTokensRemaining,
                activeModel,
                backend.isEstimatedCost,
                backend.costSource || "unknown",
                backend.usageSource || "unknown",
                backend.currency || "USD",
                backend.dataQuality || "unknown"
            );
        }
        if (backend.metrics !== undefined && backend.metrics.length > 0) {
            usageDatabase.recordProviderMetrics(providerName, backend.metrics);
        }
        syncMetricsPayload();
    }

    function recordToolUsageSnapshot(monitor) {
        if (!usageDatabase.enabled || !monitor) {
            return;
        }

        usageDatabase.recordToolSnapshot(
            monitor.toolName,
            monitor.usageCount,
            monitor.usageLimit,
            monitor.periodLabel,
            monitor.planTier,
            monitor.limitReached
        );
        syncMetricsPayload();
    }

    function syncMetricsPayload() {
        if (!metricsServer) {
            return;
        }

        var lines = [];
        var apiSpend = {};
        var apiSpendToday = {};
        var apiSpendMonth = {};
        var estimatedBurn = {};
        var subscriptionFees = 0;
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (!provider.enabled || !provider.backend) {
                continue;
            }
            var providerKey = provider.configKey;
            var backend = provider.backend;
            var costSource = labelValue(backend.costSource || "unknown");
            var usageSource = labelValue(backend.usageSource || "unknown");
            var currency = labelValue(backend.currency || "USD");
            var dataQuality = labelValue(backend.dataQuality || "unknown");
            lines.push("ai_usage_provider_connected{provider=\"" + providerKey + "\"} " + (backend.connected ? "1" : "0"));
            lines.push("ai_usage_provider_source_info{provider=\"" + providerKey + "\",cost_source=\"" + costSource + "\",usage_source=\"" + usageSource + "\",currency=\"" + currency + "\",data_quality=\"" + dataQuality + "\"} 1");
            var typedMetrics = backend.metrics || [];
            for (var metricIndex = 0; metricIndex < typedMetrics.length; metricIndex++) {
                var metric = typedMetrics[metricIndex];
                var metricLabels = "provider=\"" + providerKey
                    + "\",kind=\"" + labelValue(metric.kind)
                    + "\",unit=\"" + labelValue(metric.unit)
                    + "\",currency=\"" + labelValue(metric.currency)
                    + "\",source=\"" + labelValue(metric.source)
                    + "\",quality=\"" + labelValue(metric.quality)
                    + "\",scope=\"" + labelValue(metric.scope)
                    + "\",window=\"" + labelValue(metric.window) + "\"";
                lines.push("ai_usage_provider_metric_available{" + metricLabels + "} "
                           + (metric.available ? "1" : "0"));
                if (metric.available) {
                    lines.push("ai_usage_provider_metric{" + metricLabels + "} " + Number(metric.value));
                }
            }
            lines.push("ai_usage_provider_probe_input_tokens{provider=\"" + providerKey + "\"} " + (backend.probeInputTokens || 0));
            lines.push("ai_usage_provider_probe_output_tokens{provider=\"" + providerKey + "\"} " + (backend.probeOutputTokens || 0));
            lines.push("ai_usage_provider_probe_requests{provider=\"" + providerKey + "\"} " + (backend.probeRequestCount || 0));
            if (backend.lastRefreshed) {
                lines.push("ai_usage_provider_last_refresh_seconds{provider=\"" + providerKey + "\"} "
                           + Date.parse(backend.lastRefreshed) / 1000);
            }
            var currentCost = backend.metric ? backend.metric("cost", "", "current") : {};
            var dailyCost = backend.metric ? backend.metric("cost", "", "day") : {};
            var monthlyCost = backend.metric ? backend.metric("cost", "", "month") : {};
            if ((currentCost.source === "billing_api" || currentCost.source === "usage_api")
                    && currentCost.available) {
                addCurrencyValue(apiSpend, currentCost.currency, currentCost.value);
                if (dailyCost.available) addCurrencyValue(apiSpendToday, dailyCost.currency, dailyCost.value);
                if (monthlyCost.available) addCurrencyValue(apiSpendMonth, monthlyCost.currency, monthlyCost.value);
            } else if (backend.costSource === "estimated_from_usage" || backend.isEstimatedCost) {
                addCurrencyValue(estimatedBurn, currency,
                                 backend.estimatedMonthlyCost || backend.monthlyCost || backend.cost || 0);
            }
        }

        var tools = registry.allSubscriptionTools || [];
        for (var j = 0; j < tools.length; j++) {
            var tool = tools[j];
            if (!tool.enabled || !tool.monitor) {
                continue;
            }
            var toolKey = tool.name.toLowerCase().replace(/[^a-z0-9]+/g, "_");
            lines.push("ai_usage_tool_installed{tool=\"" + toolKey + "\"} " + (tool.monitor.installed ? "1" : "0"));
            lines.push("ai_usage_tool_usage_count{tool=\"" + toolKey + "\"} " + (tool.monitor.usageCount || 0));
            lines.push("ai_usage_tool_usage_limit{tool=\"" + toolKey + "\"} " + (tool.monitor.usageLimit || 0));
            lines.push("ai_usage_tool_percent_used{tool=\"" + toolKey + "\"} " + (tool.monitor.percentUsed || 0));
            lines.push("ai_usage_tool_last_activity_seconds{tool=\"" + toolKey + "\"} " + (tool.monitor.lastActivity ? Date.parse(tool.monitor.lastActivity) / 1000 : 0));
            lines.push("ai_usage_tool_subscription_fee{tool=\"" + toolKey + "\",cost_source=\"self_tracked\",currency=\"USD\"} " + (tool.monitor.hasSubscriptionCost ? (tool.monitor.subscriptionCost || 0) : 0));
            if (tool.monitor.hasSubscriptionCost) {
                subscriptionFees += tool.monitor.subscriptionCost || 0;
            }
        }

        appendCurrencyMetrics(lines, "ai_usage_api_spend", "period=\"current\"", apiSpend);
        appendCurrencyMetrics(lines, "ai_usage_api_spend", "period=\"today\"", apiSpendToday);
        appendCurrencyMetrics(lines, "ai_usage_api_spend", "period=\"month\"", apiSpendMonth);
        lines.push("ai_usage_subscription_fees{period=\"month\",currency=\"USD\",cost_source=\"self_tracked\"} " + subscriptionFees);
        appendCurrencyMetrics(lines, "ai_usage_estimated_burn",
                              "period=\"month\",cost_source=\"estimated_from_usage\"", estimatedBurn);

        var exposure = {};
        mergeCurrencyValues(exposure, apiSpendMonth);
        mergeCurrencyValues(exposure, estimatedBurn);
        addCurrencyValue(exposure, "USD", subscriptionFees);
        appendCurrencyMetrics(lines, "ai_usage_total_monthly_exposure", "", exposure);

        metricsServer.payload = lines.join("\n") + "\n";
    }

    function labelValue(value) {
        return (value || "").toString().replace(/\\/g, "\\\\").replace(/"/g, "\\\"");
    }

    function addCurrencyValue(totals, currency, value) {
        var code = labelValue(currency || "USD").toUpperCase();
        totals[code] = (totals[code] || 0) + Number(value || 0);
    }

    function mergeCurrencyValues(target, source) {
        var currencies = Object.keys(source || {});
        for (var i = 0; i < currencies.length; i++) {
            addCurrencyValue(target, currencies[i], source[currencies[i]]);
        }
    }

    function appendCurrencyMetrics(lines, metric, extraLabels, totals) {
        var currencies = Object.keys(totals || {}).sort();
        for (var i = 0; i < currencies.length; i++) {
            var prefix = extraLabels ? extraLabels + "," : "";
            lines.push(metric + "{" + prefix + "currency=\"" + currencies[i] + "\"} "
                       + totals[currencies[i]]);
        }
    }

    function providerRateLimitPercent(backend) {
        if (!backend) {
            return 0;
        }
        var requestPercent = -1;
        var tokenPercent = -1;
        var requestLimit = backend.metric ? backend.metric("request_limit") : {};
        var requestRemaining = backend.metric ? backend.metric("request_remaining") : {};
        var tokenLimit = backend.metric ? backend.metric("token_limit") : {};
        var tokenRemaining = backend.metric ? backend.metric("token_remaining") : {};
        if (requestLimit.available && requestRemaining.available && Number(requestLimit.value) > 0) {
            requestPercent = 100 - (Number(requestRemaining.value) * 100 / Number(requestLimit.value));
        }
        if (tokenLimit.available && tokenRemaining.available && Number(tokenLimit.value) > 0) {
            tokenPercent = 100 - (Number(tokenRemaining.value) * 100 / Number(tokenLimit.value));
        }
        return Math.max(requestPercent, tokenPercent);
    }

    function evaluateProviderQuota(displayName, backend) {
        var usedPercent = providerRateLimitPercent(backend);
        if (usedPercent >= 0 && usedPercent >= (configuration.warningThreshold || 80)) {
            notificationController.handleQuotaWarning(displayName, Math.round(usedPercent));
        }
    }

    function connectProviderSignals() {
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            var backend = provider.backend;

            backend.budgetWarning.connect(notificationController.handleBudgetWarning);
            backend.budgetExceeded.connect(notificationController.handleBudgetExceeded);
            backend.providerDisconnected.connect(notificationController.handleProviderDisconnected);
            backend.providerReconnected.connect(notificationController.handleProviderReconnected);
            backend.errorChanged.connect(makeErrorHandler(provider.name, provider.configKey, backend));
            backend.dataUpdated.connect(makeSnapshotHandler(provider.dbName, backend));
        }
    }

    function connectToolSignals() {
        var tools = registry.allSubscriptionTools || [];
        for (var i = 0; i < tools.length; i++) {
            var monitor = tools[i].monitor;
            monitor.limitWarning.connect(notificationController.handleToolLimitWarning);
            monitor.usageLimitReached.connect(notificationController.handleToolLimitReached);
            monitor.syncDiagnostic.connect(notificationController.handleToolSyncDiagnostic);
            monitor.usageUpdated.connect(makeToolSnapshotHandler(monitor));
        }
    }

    function makeErrorHandler(displayName, configKey, backend) {
        return function() {
            if (backend.error
                    && configuration.notifyOnError
                    && configuration[configKey + "NotificationsEnabled"]) {
                notificationController.sendErrorNotification(i18n("%1 Error", displayName), backend.error);
            }
        };
    }

    function makeSnapshotHandler(dbName, backend) {
        return function() {
            recordProviderSnapshot(dbName, backend);
            evaluateProviderQuota(dbName, backend);
        };
    }

    function makeToolSnapshotHandler(monitor) {
        return function() {
            recordToolUsageSnapshot(monitor);
        };
    }

    Component.onCompleted: {
        connectProviderSignals();
        connectToolSignals();
        usageDatabase.init();
        syncMetricsPayload();
        startupTimer.start();
        initialPruneTimer.start();
        if (configuration.browserSyncEnabled) {
            initialSyncTimer.start();
        }
    }

    Connections {
        target: runtime.secrets

        function onWalletOpenChanged() {
            if (runtime.secrets.walletOpen) {
                runtime.loadApiKeys(runtime.scheduler.refreshCredentialChanged, true);
            }
        }

        function onKeyStored(provider) {
            if (provider === "copilot_github"
                    || provider === "bedrock_secret_access_key"
                    || provider === "bedrock_session_token"
                    || provider === "slack_webhook_url"
                    || provider === "discord_webhook_url") {
                runtime.loadIntegrationSecrets();
                return;
            }
            var providers = runtime.registry.allProviders || [];
            for (var i = 0; i < providers.length; i++) {
                var descriptor = providers[i];
                if ((descriptor.secretKey || descriptor.configKey) === provider) {
                    runtime.loadProviderApiKey(descriptor.configKey,
                                               runtime.scheduler.refreshCredentialChanged,
                                               true);
                    return;
                }
            }
        }

        function onKeyRemoved(provider) {
            var providers = runtime.registry.allProviders || [];
            for (var i = 0; i < providers.length; i++) {
                var descriptor = providers[i];
                if ((descriptor.secretKey || descriptor.configKey) === provider) {
                    descriptor.backend.cancelRefresh();
                    descriptor.backend.setApiKey("");
                    return;
                }
            }
        }
    }

    Connections {
        target: runtime.configuration

        function providerEnabledChanged(configKey) {
            runtime.loadProviderApiKey(configKey, runtime.scheduler.refreshConfigurationChanged, true);
        }

        function syncDescriptorProvider(configKey, shouldRefresh) {
            var provider = runtime.registry.providerByConfigKey(configKey);
            if (!provider || !provider.backend) return;
            var backend = provider.backend;
            if (backend.model !== undefined)
                backend.model = runtime.configuration[configKey + "Model"] || "";
            backend.customBaseUrl = runtime.configuration[configKey + "CustomBaseUrl"] || "";
            backend.dailyBudget = (runtime.configuration[configKey + "DailyBudget"] || 0) / 100.0;
            backend.monthlyBudget = (runtime.configuration[configKey + "MonthlyBudget"] || 0) / 100.0;
            if (shouldRefresh)
                runtime.loadProviderApiKey(configKey, runtime.scheduler.refreshConfigurationChanged, true);
        }

        function onOpenaiEnabledChanged() { providerEnabledChanged("openai"); }
        function onAnthropicEnabledChanged() { providerEnabledChanged("anthropic"); }
        function onGoogleEnabledChanged() { providerEnabledChanged("google"); }
        function onMistralEnabledChanged() { providerEnabledChanged("mistral"); }
        function onDeepseekEnabledChanged() { providerEnabledChanged("deepseek"); }
        function onGroqEnabledChanged() { providerEnabledChanged("groq"); }
        function onXaiEnabledChanged() { providerEnabledChanged("xai"); }
        function onOllamaEnabledChanged() { providerEnabledChanged("ollama"); }
        function onOpenrouterEnabledChanged() { providerEnabledChanged("openrouter"); }
        function onTogetherEnabledChanged() { providerEnabledChanged("together"); }
        function onCohereEnabledChanged() { providerEnabledChanged("cohere"); }
        function onGoogleveoEnabledChanged() { providerEnabledChanged("googleveo"); }
        function onAzureEnabledChanged() { providerEnabledChanged("azure"); }
        function onBedrockEnabledChanged() { providerEnabledChanged("bedrock"); }
        function onLitellmEnabledChanged() { providerEnabledChanged("litellm"); }
        function onCerebrasEnabledChanged() { providerEnabledChanged("cerebras"); }
        function onFireworksEnabledChanged() { providerEnabledChanged("fireworks"); }
        function onPerplexityEnabledChanged() { providerEnabledChanged("perplexity"); }

        function onLitellmModelChanged() { syncDescriptorProvider("litellm", true); }
        function onCerebrasModelChanged() { syncDescriptorProvider("cerebras", true); }
        function onFireworksModelChanged() { syncDescriptorProvider("fireworks", true); }
        function onPerplexityModelChanged() { syncDescriptorProvider("perplexity", true); }
        function onLitellmCustomBaseUrlChanged() { syncDescriptorProvider("litellm", true); }
        function onCerebrasCustomBaseUrlChanged() { syncDescriptorProvider("cerebras", true); }
        function onFireworksCustomBaseUrlChanged() { syncDescriptorProvider("fireworks", true); }
        function onPerplexityCustomBaseUrlChanged() { syncDescriptorProvider("perplexity", true); }
        function onLitellmDailyBudgetChanged() { syncDescriptorProvider("litellm", false); }
        function onLitellmMonthlyBudgetChanged() { syncDescriptorProvider("litellm", false); }
        function onCerebrasDailyBudgetChanged() { syncDescriptorProvider("cerebras", false); }
        function onCerebrasMonthlyBudgetChanged() { syncDescriptorProvider("cerebras", false); }
        function onFireworksDailyBudgetChanged() { syncDescriptorProvider("fireworks", false); }
        function onFireworksMonthlyBudgetChanged() { syncDescriptorProvider("fireworks", false); }
        function onPerplexityDailyBudgetChanged() { syncDescriptorProvider("perplexity", false); }
        function onPerplexityMonthlyBudgetChanged() { syncDescriptorProvider("perplexity", false); }

        function onClaudeCodeEnabledChanged() {
            if (runtime.claudeCodeMonitor.enabled) {
                runtime.claudeCodeMonitor.checkToolInstalled();
            }
        }

        function onCodexEnabledChanged() {
            if (runtime.codexCliMonitor.enabled) {
                runtime.codexCliMonitor.checkToolInstalled();
            }
        }

        function onCopilotEnabledChanged() {
            if (runtime.copilotMonitor.enabled) {
                runtime.loadIntegrationSecrets();
                runtime.copilotMonitor.checkToolInstalled();
            }
        }

        function onCopilotOrgNameChanged() {
            runtime.loadIntegrationSecrets();
            if (runtime.copilotMonitor.enabled) {
                runtime.copilotMonitor.fetchOrgMetrics();
            }
        }

        function onCursorEnabledChanged() {
            if (runtime.cursorMonitor.enabled) {
                runtime.cursorMonitor.checkToolInstalled();
            }
        }

        function onWindsurfEnabledChanged() {
            if (runtime.windsurfMonitor.enabled) {
                runtime.windsurfMonitor.checkToolInstalled();
            }
        }

        function onJetbrainsAiEnabledChanged() {
            if (runtime.jetbrainsAiMonitor.enabled) {
                runtime.jetbrainsAiMonitor.checkToolInstalled();
            }
        }
    }

    Timer {
        id: startupTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (runtime.secrets.walletOpen) {
                runtime.loadApiKeys(runtime.scheduler.refreshStartup, false);
                runtime.scheduler.refreshAll(runtime.scheduler.refreshStartup);
            } else {
                runtime.scheduler.refreshAll(runtime.scheduler.refreshStartup);
            }
        }
    }

    Timer {
        id: initialPruneTimer
        interval: 2000
        repeat: false
        onTriggered: runtime.usageDatabase.pruneOldData()
    }

    Timer {
        id: initialSyncTimer
        interval: 5000
        repeat: false
        onTriggered: runtime.scheduler.performBrowserSync()
    }

}
