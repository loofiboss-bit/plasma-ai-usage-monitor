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

    function loadApiKeys() {
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider.enabled && provider.requiresApiKey !== false) {
                var keySlot = provider.configKey;
                if (provider.configKey === "bedrock") {
                    keySlot = "bedrock_access_key_id";
                }
                var key = secrets.getKey(keySlot);
                if (key) {
                    provider.backend.setApiKey(key);
                }
            }
        }

        if (registry.bedrockBackend) {
            registry.bedrockBackend.secretAccessKey = secrets.getKey("bedrock_secret_access_key");
            registry.bedrockBackend.sessionToken = secrets.getKey("bedrock_session_token");
        }

        webhookNotifier.slackWebhookUrl = secrets.getKey("slack_webhook_url");
        webhookNotifier.discordWebhookUrl = secrets.getKey("discord_webhook_url");

        scheduler.refreshAll();
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
            backend.isEstimatedCost
        );
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
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (!provider.enabled || !provider.backend) {
                continue;
            }
            var providerKey = provider.configKey;
            var backend = provider.backend;
            lines.push("ai_usage_provider_connected{provider=\"" + providerKey + "\"} " + (backend.connected ? "1" : "0"));
            lines.push("ai_usage_provider_cost{provider=\"" + providerKey + "\"} " + (backend.cost || 0));
            lines.push("ai_usage_provider_daily_cost{provider=\"" + providerKey + "\"} " + (backend.dailyCost || 0));
            lines.push("ai_usage_provider_monthly_cost{provider=\"" + providerKey + "\"} " + (backend.monthlyCost || 0));
            lines.push("ai_usage_provider_input_tokens{provider=\"" + providerKey + "\"} " + (backend.inputTokens || 0));
            lines.push("ai_usage_provider_output_tokens{provider=\"" + providerKey + "\"} " + (backend.outputTokens || 0));
            lines.push("ai_usage_provider_requests{provider=\"" + providerKey + "\"} " + (backend.requestCount || 0));
            lines.push("ai_usage_provider_rate_limit_requests{provider=\"" + providerKey + "\"} " + (backend.rateLimitRequests || 0));
            lines.push("ai_usage_provider_rate_limit_requests_remaining{provider=\"" + providerKey + "\"} " + (backend.rateLimitRequestsRemaining || 0));
            lines.push("ai_usage_provider_rate_limit_tokens{provider=\"" + providerKey + "\"} " + (backend.rateLimitTokens || 0));
            lines.push("ai_usage_provider_rate_limit_tokens_remaining{provider=\"" + providerKey + "\"} " + (backend.rateLimitTokensRemaining || 0));
            lines.push("ai_usage_provider_last_refresh_seconds{provider=\"" + providerKey + "\"} " + (backend.lastRefreshed ? Date.parse(backend.lastRefreshed) / 1000 : 0));
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
        }

        metricsServer.payload = lines.join("\n") + "\n";
    }

    function connectProviderSignals() {
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            var backend = provider.backend;

            backend.quotaWarning.connect(notificationController.handleQuotaWarning);
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
                runtime.loadApiKeys();
            }
        }
    }

    Connections {
        target: runtime.configuration

        function onOpenaiEnabledChanged() { runtime.loadApiKeys(); }
        function onAnthropicEnabledChanged() { runtime.loadApiKeys(); }
        function onGoogleEnabledChanged() { runtime.loadApiKeys(); }
        function onMistralEnabledChanged() { runtime.loadApiKeys(); }
        function onDeepseekEnabledChanged() { runtime.loadApiKeys(); }
        function onGroqEnabledChanged() { runtime.loadApiKeys(); }
        function onXaiEnabledChanged() { runtime.loadApiKeys(); }
        function onOllamaEnabledChanged() { runtime.loadApiKeys(); }
        function onOpenrouterEnabledChanged() { runtime.loadApiKeys(); }
        function onTogetherEnabledChanged() { runtime.loadApiKeys(); }
        function onCohereEnabledChanged() { runtime.loadApiKeys(); }
        function onGoogleveoEnabledChanged() { runtime.loadApiKeys(); }
        function onAzureEnabledChanged() { runtime.loadApiKeys(); }
        function onBedrockEnabledChanged() { runtime.loadApiKeys(); }

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
                runtime.copilotMonitor.checkToolInstalled();
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
                runtime.loadApiKeys();
            } else {
                runtime.scheduler.refreshAll();
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
