import QtQuick
import com.github.loofi.aiusagemonitor 1.0
import "Utils.js" as Utils

QtObject {
    id: registry

    required property var configuration

    required property var openaiBackend
    required property var anthropicBackend
    required property var googleBackend
    required property var mistralBackend
    required property var deepseekBackend
    required property var groqBackend
    required property var xaiBackend
    required property var ollamaBackend
    required property var openrouterBackend
    required property var togetherBackend
    required property var cohereBackend
    required property var googleveoBackend
    required property var azureBackend
    required property var bedrockBackend
    required property var providerManager

    required property var claudeCodeMonitor
    required property var codexCliMonitor
    required property var copilotMonitor
    required property var cursorMonitor
    required property var windsurfMonitor
    required property var jetbrainsAiMonitor

    property ProviderCatalog providerCatalog: ProviderCatalog {}
    readonly property bool demoMode: AppInfo.demoMode
    function backendForConfigKey(configKey) {
        return providerManager.backend(configKey);
    }

    function providerEnabled(descriptor) {
        var configKey = descriptor.configKey;
        if (demoMode) {
            return ["litellm", "cerebras", "fireworks", "perplexity"].indexOf(configKey) >= 0;
        }
        return !!configuration[providerEnabledKey(descriptor)];
    }

    function providerEnabledKey(descriptor) {
        return descriptor.enabledConfigKey || descriptor.configKey + "Enabled";
    }

    function providerModelKey(descriptor) {
        return descriptor.modelConfigKey || descriptor.configKey + "Model";
    }

    function providerBaseUrlKey(descriptor) {
        return descriptor.customBaseUrlConfigKey || descriptor.configKey + "CustomBaseUrl";
    }

    function providerSecretKey(descriptor) {
        return descriptor.secretKey || (descriptor.configKey === "bedrock" ? "bedrock_access_key_id" : descriptor.configKey);
    }

    function providerRefreshInterval(descriptor) {
        return configuration[descriptor.refreshConfigKey] || 0;
    }

    function providerNotificationsEnabled(descriptor) {
        return !!configuration[descriptor.notificationsConfigKey];
    }

    readonly property var allProviders: {
        var descriptors = providerCatalog.providers;
        var providers = [];
        for (var i = 0; i < descriptors.length; i++) {
            var descriptor = descriptors[i];
            providers.push({
                name: descriptor.name,
                label: descriptor.label,
                dbName: descriptor.dbName,
                configKey: descriptor.configKey,
                backend: backendForConfigKey(descriptor.configKey),
                enabledKey: providerEnabledKey(descriptor),
                modelKey: providerModelKey(descriptor),
                customBaseUrlKey: providerBaseUrlKey(descriptor),
                refreshKey: descriptor.refreshConfigKey,
                notificationsKey: descriptor.notificationsConfigKey,
                dailyBudgetKey: descriptor.dailyBudgetConfigKey,
                monthlyBudgetKey: descriptor.monthlyBudgetConfigKey,
                secretKey: providerSecretKey(descriptor),
                catalogKey: descriptor.catalogKey || descriptor.configKey,
                enabled: providerEnabled(descriptor),
                color: descriptor.color,
                iconSource: Qt.resolvedUrl("../icons/providers/" + descriptor.configKey + ".svg"),
                requiresApiKey: descriptor.requiresApiKey,
                refreshInterval: providerRefreshInterval(descriptor),
                minimumRefreshSeconds: descriptor.minimumRefreshSeconds,
                requestBudget: descriptor.requestBudget,
                monitoringLevel: descriptor.monitoringLevel,
                safeRefresh: descriptor.safeRefresh,
                notificationsEnabled: providerNotificationsEnabled(descriptor)
            });
        }
        return providers;
    }

    readonly property var allSubscriptionTools: [
        {
            name: "Claude Code",
            monitor: claudeCodeMonitor,
            enabled: demoMode || configuration.claudeCodeEnabled,
            notify: configuration.claudeCodeNotifications,
            iconSource: Qt.resolvedUrl("../icons/tools/claude-code.svg")
        },
        {
            name: "Codex CLI",
            monitor: codexCliMonitor,
            enabled: demoMode || configuration.codexEnabled,
            notify: configuration.codexNotifications,
            iconSource: Qt.resolvedUrl("../icons/tools/codex-cli.svg")
        },
        {
            name: "GitHub Copilot",
            monitor: copilotMonitor,
            enabled: demoMode || configuration.copilotEnabled,
            notify: configuration.copilotNotifications,
            iconSource: Qt.resolvedUrl("../icons/tools/copilot.svg")
        },
        {
            name: "Cursor",
            monitor: cursorMonitor,
            enabled: configuration.cursorEnabled,
            notify: configuration.cursorNotifications,
            iconSource: Qt.resolvedUrl("../icons/tools/cursor.svg")
        },
        {
            name: "Windsurf",
            monitor: windsurfMonitor,
            enabled: configuration.windsurfEnabled,
            notify: configuration.windsurfNotifications,
            iconSource: Qt.resolvedUrl("../icons/tools/windsurf.svg")
        },
        {
            name: "JetBrains AI",
            monitor: jetbrainsAiMonitor,
            enabled: configuration.jetbrainsAiEnabled,
            notify: configuration.jetbrainsAiNotifications,
            iconSource: Qt.resolvedUrl("../icons/tools/jetbrains.svg")
        }
    ]

    readonly property int enabledToolCount: {
        var count = 0;
        for (var i = 0; i < allSubscriptionTools.length; i++) {
            if (allSubscriptionTools[i].enabled) {
                count++;
            }
        }
        return count;
    }

    readonly property int connectedCount: {
        var count = 0;
        for (var i = 0; i < allProviders.length; i++) {
            if (allProviders[i].enabled && allProviders[i].backend && allProviders[i].backend.connected) {
                count++;
            }
        }
        return count;
    }

    readonly property var actualSpendByCurrency: {
        var totals = {};
        for (var i = 0; i < allProviders.length; i++) {
            if (allProviders[i].enabled && allProviders[i].backend && allProviders[i].backend.connected) {
                var source = allProviders[i].backend.costSource || "unknown";
                if (source === "billing_api" || source === "usage_api" || source === "actual_api") {
                    Utils.addCurrencyTotal(totals,
                                           allProviders[i].backend.currency,
                                           allProviders[i].backend.cost);
                }
            }
        }
        return totals;
    }

    readonly property var actualSpendCurrencies: Object.keys(actualSpendByCurrency).sort()
    readonly property bool hasMixedCurrencies: actualSpendCurrencies.length > 1
    readonly property string totalCostLabel: Utils.formatCurrencyTotals(actualSpendByCurrency)
    readonly property double totalCost: {
        if (actualSpendCurrencies.length !== 1) return 0;
        return actualSpendByCurrency[actualSpendCurrencies[0]] || 0;
    }

    function formatCompactMetric(value) {
        if (value >= 1000000) {
            return (value / 1000000).toFixed(1) + "M";
        }
        if (value >= 1000) {
            return (value / 1000).toFixed(1) + "K";
        }
        return value.toString();
    }

    function providerConfigKey(providerName) {
        for (var i = 0; i < allProviders.length; i++) {
            var provider = allProviders[i];
            if (provider.name === providerName
                    || provider.dbName === providerName
                    || provider.name.indexOf(providerName) === 0
                    || providerName.indexOf(provider.name) === 0) {
                return provider.configKey;
            }
        }
        return "";
    }

    function providerByConfigKey(configKey) {
        for (var i = 0; i < allProviders.length; i++) {
            if (allProviders[i].configKey === configKey) {
                return allProviders[i];
            }
        }
        return null;
    }

    function isProviderNotificationEnabled(providerName) {
        var configKey = providerConfigKey(providerName);
        if (configKey === "") {
            return true;
        }

        var provider = providerByConfigKey(configKey);
        return provider ? provider.notificationsEnabled : true;
    }

    function isToolNotificationEnabled(toolName) {
        for (var i = 0; i < allSubscriptionTools.length; i++) {
            if (allSubscriptionTools[i].name === toolName) {
                return allSubscriptionTools[i].notify;
            }
        }
        return true;
    }
}
