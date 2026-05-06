import QtQuick
import com.github.loofi.aiusagemonitor 1.0

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

    required property var claudeCodeMonitor
    required property var codexCliMonitor
    required property var copilotMonitor
    required property var cursorMonitor
    required property var windsurfMonitor
    required property var jetbrainsAiMonitor

    property ProviderCatalog providerCatalog: ProviderCatalog {}
    readonly property bool demoMode: AppInfo.demoMode

    function backendForConfigKey(configKey) {
        switch (configKey) {
        case "openai":
            return openaiBackend;
        case "anthropic":
            return anthropicBackend;
        case "google":
            return googleBackend;
        case "mistral":
            return mistralBackend;
        case "deepseek":
            return deepseekBackend;
        case "groq":
            return groqBackend;
        case "xai":
            return xaiBackend;
        case "ollama":
            return ollamaBackend;
        case "openrouter":
            return openrouterBackend;
        case "together":
            return togetherBackend;
        case "cohere":
            return cohereBackend;
        case "googleveo":
            return googleveoBackend;
        case "azure":
            return azureBackend;
        case "bedrock":
            return bedrockBackend;
        default:
            return null;
        }
    }

    function providerEnabled(configKey) {
        if (demoMode) {
            return ["openai", "google", "mistral", "deepseek", "groq", "openrouter"].indexOf(configKey) >= 0;
        }

        switch (configKey) {
        case "openai":
            return configuration.openaiEnabled;
        case "anthropic":
            return configuration.anthropicEnabled;
        case "google":
            return configuration.googleEnabled;
        case "mistral":
            return configuration.mistralEnabled;
        case "deepseek":
            return configuration.deepseekEnabled;
        case "groq":
            return configuration.groqEnabled;
        case "xai":
            return configuration.xaiEnabled;
        case "ollama":
            return configuration.ollamaEnabled;
        case "openrouter":
            return configuration.openrouterEnabled;
        case "together":
            return configuration.togetherEnabled;
        case "cohere":
            return configuration.cohereEnabled;
        case "googleveo":
            return configuration.googleveoEnabled;
        case "azure":
            return configuration.azureEnabled;
        case "bedrock":
            return configuration.bedrockEnabled;
        default:
            return false;
        }
    }

    function providerRefreshInterval(configKey) {
        switch (configKey) {
        case "openai":
            return configuration.openaiRefreshInterval;
        case "anthropic":
            return configuration.anthropicRefreshInterval;
        case "google":
            return configuration.googleRefreshInterval;
        case "mistral":
            return configuration.mistralRefreshInterval;
        case "deepseek":
            return configuration.deepseekRefreshInterval;
        case "groq":
            return configuration.groqRefreshInterval;
        case "xai":
            return configuration.xaiRefreshInterval;
        case "ollama":
            return configuration.ollamaRefreshInterval;
        case "openrouter":
            return configuration.openrouterRefreshInterval;
        case "together":
            return configuration.togetherRefreshInterval;
        case "cohere":
            return configuration.cohereRefreshInterval;
        case "googleveo":
            return configuration.googleveoRefreshInterval;
        case "azure":
            return configuration.azureRefreshInterval;
        case "bedrock":
            return configuration.bedrockRefreshInterval;
        default:
            return 0;
        }
    }

    function providerNotificationsEnabled(configKey) {
        switch (configKey) {
        case "openai":
            return configuration.openaiNotificationsEnabled;
        case "anthropic":
            return configuration.anthropicNotificationsEnabled;
        case "google":
            return configuration.googleNotificationsEnabled;
        case "mistral":
            return configuration.mistralNotificationsEnabled;
        case "deepseek":
            return configuration.deepseekNotificationsEnabled;
        case "groq":
            return configuration.groqNotificationsEnabled;
        case "xai":
            return configuration.xaiNotificationsEnabled;
        case "ollama":
            return configuration.ollamaNotificationsEnabled;
        case "openrouter":
            return configuration.openrouterNotificationsEnabled;
        case "together":
            return configuration.togetherNotificationsEnabled;
        case "cohere":
            return configuration.cohereNotificationsEnabled;
        case "googleveo":
            return configuration.googleveoNotificationsEnabled;
        case "azure":
            return configuration.azureNotificationsEnabled;
        case "bedrock":
            return configuration.bedrockNotificationsEnabled;
        default:
            return false;
        }
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
                enabled: providerEnabled(descriptor.configKey),
                color: descriptor.color,
                iconSource: Qt.resolvedUrl("../icons/providers/" + descriptor.configKey + ".svg"),
                requiresApiKey: descriptor.requiresApiKey,
                refreshInterval: providerRefreshInterval(descriptor.configKey),
                notificationsEnabled: providerNotificationsEnabled(descriptor.configKey)
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

    readonly property double totalCost: {
        var total = 0;
        for (var i = 0; i < allProviders.length; i++) {
            if (allProviders[i].enabled && allProviders[i].backend && allProviders[i].backend.connected) {
                total += allProviders[i].backend.cost;
            }
        }
        for (var j = 0; j < allSubscriptionTools.length; j++) {
            if (allSubscriptionTools[j].enabled
                    && allSubscriptionTools[j].monitor
                    && allSubscriptionTools[j].monitor.hasSubscriptionCost) {
                total += allSubscriptionTools[j].monitor.subscriptionCost;
            }
        }
        return total;
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
