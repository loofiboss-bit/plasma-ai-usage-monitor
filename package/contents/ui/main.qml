import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "Utils.js" as Utils

PlasmoidItem {
    id: root
    property string modelMigrationNotice: ""
    readonly property bool pluginVersionMismatch: {
        var required = plasmoid.metaData?.version || "";
        return required !== "" && required !== AppInfo.version;
    }

    switchWidth: Kirigami.Units.gridUnit * 12
    switchHeight: Kirigami.Units.gridUnit * 12

    toolTipMainText: i18n("AI Usage Monitor")
    toolTipSubText: {
        var lines = [];
        var providers = root.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider.enabled && provider.backend && provider.backend.connected) {
                var info = provider.name + ": ";
                if (provider.backend.cost > 0) {
                    info += Utils.formatMoney(provider.backend.cost, provider.backend.currency || "USD") + " | ";
                }
                if ((provider.backend.rateLimitRequestsRemaining || 0) > 0) {
                    info += provider.backend.rateLimitRequestsRemaining + " req left";
                } else if ((provider.backend.rateLimitTokensRemaining || 0) > 0) {
                    info += provider.backend.rateLimitTokensRemaining + " tokens left";
                } else {
                    info += i18n("Healthy");
                }
                lines.push(info);
            } else if (provider.enabled && provider.backend && provider.backend.error) {
                lines.push(provider.name + ": " + i18n("Error"));
            }
        }
        var tools = root.allSubscriptionTools || [];
        for (var j = 0; j < tools.length; j++) {
            var tool = tools[j];
            if (tool.enabled && tool.monitor && tool.monitor.installed) {
                var toolInfo = tool.name + ": ";
                if (tool.monitor.limitReached) {
                    toolInfo += i18n("Limit Reached");
                } else if ((tool.monitor.usageLimit || 0) > 0) {
                    toolInfo += tool.monitor.usageCount + "/" + tool.monitor.usageLimit + " " + i18n("used");
                } else {
                    toolInfo += i18n("Active");
                }
                lines.push(toolInfo);
            }
        }
        return lines.length > 0 ? lines.join("\n") : i18n("Click to configure providers");
    }

    property alias openai: openaiBackend
    property alias anthropic: anthropicBackend
    property alias google: googleBackend
    property alias mistral: mistralBackend
    property alias deepseek: deepseekBackend
    property alias groq: groqBackend
    property alias xai: xaiBackend
    property alias ollama: ollamaBackend
    property alias openrouter: openrouterBackend
    property alias together: togetherBackend
    property alias cohere: cohereBackend
    property alias googleveo: googleveoBackend
    property alias azure: azureBackend
    property alias bedrock: bedrockBackend
    property alias usageDb: usageDatabase
    property alias refreshScheduler: refreshScheduler

    property alias claudeCode: claudeCodeMonitor
    property alias codexCli: codexCliMonitor
    property alias copilot: copilotMonitor
    property alias cursor: cursorMonitor
    property alias windsurf: windsurfMonitor
    property alias jetbrainsAi: jetbrainsAiMonitor
    property alias intelligenceEngine: analystIntelligence

    readonly property var allProviders: providerRegistry.allProviders
    readonly property var allSubscriptionTools: providerRegistry.allSubscriptionTools
    readonly property int enabledToolCount: providerRegistry.enabledToolCount
    readonly property int connectedCount: providerRegistry.connectedCount
    readonly property double totalCost: providerRegistry.totalCost
    readonly property string totalCostLabel: providerRegistry.totalCostLabel
    readonly property bool hasMixedCurrencies: providerRegistry.hasMixedCurrencies

    function formatCompactMetric(value) {
        return providerRegistry.formatCompactMetric(value);
    }

    function selectedPlanTier(monitor, planId, legacyIndex) {
        if (planId && planId.length > 0) {
            return planId;
        }
        var plans = monitor.availablePlans();
        if (legacyIndex >= 0 && legacyIndex < plans.length) {
            return monitor.planIdForLabel(plans[legacyIndex]);
        }
        return plans.length > 0 ? monitor.planIdForLabel(plans[0]) : "";
    }

    function applySubscriptionPlan(monitor, planId, legacyIndex, customLimit) {
        if (!monitor) {
            return;
        }
        var resolvedPlan = root.selectedPlanTier(monitor, planId, legacyIndex);
        monitor.planTier = resolvedPlan;

        if (customLimit > 0) {
            monitor.usageLimit = customLimit;
        } else {
            monitor.usageLimit = monitor.defaultLimitForPlan(resolvedPlan);
        }

        if (monitor.hasSecondaryLimit) {
            monitor.secondaryUsageLimit = monitor.defaultSecondaryLimitForPlan(resolvedPlan);
        }
    }

    function refreshAll() {
        refreshScheduler.refreshAll();
    }

    function performBrowserSync() {
        refreshScheduler.performBrowserSync();
    }

    function generateAnalystInsight() {
        if (!analystIntelligence) {
            return;
        }

        var activity = usageDatabase.getYearlyActivity(plasmoid.configuration.analystIntensityMode);
        var efficiency = usageDatabase.getEfficiencySeries(14);
        var overview = usageDatabase.getAnalystOverview(30);

        analystIntelligence.generateInsight(activity.days, efficiency, overview);
    }

    SecretsManager {
        id: secrets
    }

    UsageDatabase {
        id: usageDatabase
        enabled: plasmoid.configuration.historyEnabled
        retentionDays: plasmoid.configuration.historyRetentionDays
    }

    OpenAIProvider {
        id: openaiBackend
        model: plasmoid.configuration.openaiModel
        projectId: plasmoid.configuration.openaiProjectId
        customBaseUrl: plasmoid.configuration.openaiCustomBaseUrl
        dailyBudget: plasmoid.configuration.openaiDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.openaiMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    AzureOpenAIProvider {
        id: azureBackend
        model: plasmoid.configuration.azureModel
        deploymentId: plasmoid.configuration.azureDeploymentId
        customBaseUrl: plasmoid.configuration.azureCustomBaseUrl
        dailyBudget: plasmoid.configuration.azureDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.azureMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    BedrockProvider {
        id: bedrockBackend
        region: plasmoid.configuration.bedrockRegion
        model: plasmoid.configuration.bedrockModel
        customBaseUrl: plasmoid.configuration.bedrockCustomBaseUrl
        dailyBudget: plasmoid.configuration.bedrockDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.bedrockMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    AnthropicProvider {
        id: anthropicBackend
        model: plasmoid.configuration.anthropicModel
        customBaseUrl: plasmoid.configuration.anthropicCustomBaseUrl
        dailyBudget: plasmoid.configuration.anthropicDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.anthropicMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    GoogleProvider {
        id: googleBackend
        model: plasmoid.configuration.googleModel
        tier: plasmoid.configuration.googleTier
        customBaseUrl: plasmoid.configuration.googleCustomBaseUrl
        dailyBudget: plasmoid.configuration.googleDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.googleMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    MistralProvider {
        id: mistralBackend
        model: plasmoid.configuration.mistralModel
        customBaseUrl: plasmoid.configuration.mistralCustomBaseUrl
        dailyBudget: plasmoid.configuration.mistralDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.mistralMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    DeepSeekProvider {
        id: deepseekBackend
        model: plasmoid.configuration.deepseekModel
        customBaseUrl: plasmoid.configuration.deepseekCustomBaseUrl
        dailyBudget: plasmoid.configuration.deepseekDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.deepseekMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    GroqProvider {
        id: groqBackend
        model: plasmoid.configuration.groqModel
        customBaseUrl: plasmoid.configuration.groqCustomBaseUrl
        dailyBudget: plasmoid.configuration.groqDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.groqMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    XAIProvider {
        id: xaiBackend
        model: plasmoid.configuration.xaiModel
        customBaseUrl: plasmoid.configuration.xaiCustomBaseUrl
        dailyBudget: plasmoid.configuration.xaiDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.xaiMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    OllamaCloudProvider {
        id: ollamaBackend
        model: plasmoid.configuration.ollamaModel
        customBaseUrl: plasmoid.configuration.ollamaCustomBaseUrl
        dailyBudget: plasmoid.configuration.ollamaDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.ollamaMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    OpenRouterProvider {
        id: openrouterBackend
        model: plasmoid.configuration.openrouterModel
        customBaseUrl: plasmoid.configuration.openrouterCustomBaseUrl
        dailyBudget: plasmoid.configuration.openrouterDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.openrouterMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    TogetherProvider {
        id: togetherBackend
        model: plasmoid.configuration.togetherModel
        customBaseUrl: plasmoid.configuration.togetherCustomBaseUrl
        dailyBudget: plasmoid.configuration.togetherDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.togetherMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    CohereProvider {
        id: cohereBackend
        model: plasmoid.configuration.cohereModel
        customBaseUrl: plasmoid.configuration.cohereCustomBaseUrl
        dailyBudget: plasmoid.configuration.cohereDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.cohereMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    GoogleVeoProvider {
        id: googleveoBackend
        model: plasmoid.configuration.googleveoModel
        tier: plasmoid.configuration.googleveoTier
        customBaseUrl: plasmoid.configuration.googleveoCustomBaseUrl
        dailyBudget: plasmoid.configuration.googleveoDailyBudget / 100.0
        monthlyBudget: plasmoid.configuration.googleveoMonthlyBudget / 100.0
        budgetWarningPercent: plasmoid.configuration.budgetWarningPercent
    }

    ProviderManager {
        id: providerManager

        function configureDescriptorBackend(key) {
            var backend = providerManager.backend(key);
            if (!backend) return;
            var modelKey = key + "Model";
            var urlKey = key + "CustomBaseUrl";
            if (backend.model !== undefined) backend.model = plasmoid.configuration[modelKey] || "";
            backend.customBaseUrl = plasmoid.configuration[urlKey] || "";
            backend.dailyBudget = (plasmoid.configuration[key + "DailyBudget"] || 0) / 100.0;
            backend.monthlyBudget = (plasmoid.configuration[key + "MonthlyBudget"] || 0) / 100.0;
            backend.budgetWarningPercent = plasmoid.configuration.budgetWarningPercent;
        }

        Component.onCompleted: {
            registerBackend("openai", openaiBackend);
            registerBackend("anthropic", anthropicBackend);
            registerBackend("google", googleBackend);
            registerBackend("mistral", mistralBackend);
            registerBackend("deepseek", deepseekBackend);
            registerBackend("groq", groqBackend);
            registerBackend("xai", xaiBackend);
            registerBackend("ollama", ollamaBackend);
            registerBackend("openrouter", openrouterBackend);
            registerBackend("together", togetherBackend);
            registerBackend("cohere", cohereBackend);
            registerBackend("googleveo", googleveoBackend);
            registerBackend("azure", azureBackend);
            registerBackend("bedrock", bedrockBackend);
            ["litellm", "cerebras", "fireworks", "perplexity"].forEach(configureDescriptorBackend);
        }
    }

    BrowserSyncService {
        id: browserSyncService
        browserType: plasmoid.configuration.browserSyncBrowser
        selectedFirefoxProfile: plasmoid.configuration.browserSyncProfile
    }

    ClaudeCodeMonitor {
        id: claudeCodeMonitor
        enabled: plasmoid.configuration.claudeCodeEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            syncEnabled = Qt.binding(function() { return plasmoid.configuration.browserSyncEnabled; });
            root.applySubscriptionPlan(claudeCodeMonitor, plasmoid.configuration.claudeCodePlanId, plasmoid.configuration.claudeCodePlan, plasmoid.configuration.claudeCodeCustomLimit);
        }
    }

    CodexCliMonitor {
        id: codexCliMonitor
        enabled: plasmoid.configuration.codexEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            syncEnabled = Qt.binding(function() { return plasmoid.configuration.browserSyncEnabled; });
            root.applySubscriptionPlan(codexCliMonitor, plasmoid.configuration.codexPlanId, plasmoid.configuration.codexPlan, plasmoid.configuration.codexCustomLimit);
        }
    }

    CopilotMonitor {
        id: copilotMonitor
        enabled: plasmoid.configuration.copilotEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold
        orgName: plasmoid.configuration.copilotOrgName
        billingMode: plasmoid.configuration.copilotBillingMode || "auto"
        monthlyResetDay: plasmoid.configuration.copilotResetDay || 1

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(copilotMonitor, plasmoid.configuration.copilotPlanId, plasmoid.configuration.copilotPlan, plasmoid.configuration.copilotCustomLimit);
            fetchOrgMetrics();
        }
    }

    CursorMonitor {
        id: cursorMonitor
        enabled: plasmoid.configuration.cursorEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(cursorMonitor, plasmoid.configuration.cursorPlanId, plasmoid.configuration.cursorPlan, plasmoid.configuration.cursorCustomLimit);
        }
    }

    WindsurfMonitor {
        id: windsurfMonitor
        enabled: plasmoid.configuration.windsurfEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(windsurfMonitor, plasmoid.configuration.windsurfPlanId, plasmoid.configuration.windsurfPlan, plasmoid.configuration.windsurfCustomLimit);
        }
    }

    JetBrainsAiMonitor {
        id: jetbrainsAiMonitor
        enabled: plasmoid.configuration.jetbrainsAiEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(jetbrainsAiMonitor, plasmoid.configuration.jetbrainsAiPlanId, plasmoid.configuration.jetbrainsAiPlan, plasmoid.configuration.jetbrainsAiCustomLimit);
        }
    }

    Connections {
        target: plasmoid.configuration

        function onClaudeCodePlanIdChanged() { root.applySubscriptionPlan(claudeCodeMonitor, plasmoid.configuration.claudeCodePlanId, plasmoid.configuration.claudeCodePlan, plasmoid.configuration.claudeCodeCustomLimit); }
        function onClaudeCodePlanChanged() { root.applySubscriptionPlan(claudeCodeMonitor, plasmoid.configuration.claudeCodePlanId, plasmoid.configuration.claudeCodePlan, plasmoid.configuration.claudeCodeCustomLimit); }
        function onClaudeCodeCustomLimitChanged() { root.applySubscriptionPlan(claudeCodeMonitor, plasmoid.configuration.claudeCodePlanId, plasmoid.configuration.claudeCodePlan, plasmoid.configuration.claudeCodeCustomLimit); }

        function onCodexPlanIdChanged() { root.applySubscriptionPlan(codexCliMonitor, plasmoid.configuration.codexPlanId, plasmoid.configuration.codexPlan, plasmoid.configuration.codexCustomLimit); }
        function onCodexPlanChanged() { root.applySubscriptionPlan(codexCliMonitor, plasmoid.configuration.codexPlanId, plasmoid.configuration.codexPlan, plasmoid.configuration.codexCustomLimit); }
        function onCodexCustomLimitChanged() { root.applySubscriptionPlan(codexCliMonitor, plasmoid.configuration.codexPlanId, plasmoid.configuration.codexPlan, plasmoid.configuration.codexCustomLimit); }

        function onCopilotPlanIdChanged() { root.applySubscriptionPlan(copilotMonitor, plasmoid.configuration.copilotPlanId, plasmoid.configuration.copilotPlan, plasmoid.configuration.copilotCustomLimit); }
        function onCopilotPlanChanged() { root.applySubscriptionPlan(copilotMonitor, plasmoid.configuration.copilotPlanId, plasmoid.configuration.copilotPlan, plasmoid.configuration.copilotCustomLimit); }
        function onCopilotCustomLimitChanged() { root.applySubscriptionPlan(copilotMonitor, plasmoid.configuration.copilotPlanId, plasmoid.configuration.copilotPlan, plasmoid.configuration.copilotCustomLimit); }

        function onCursorPlanIdChanged() { root.applySubscriptionPlan(cursorMonitor, plasmoid.configuration.cursorPlanId, plasmoid.configuration.cursorPlan, plasmoid.configuration.cursorCustomLimit); }
        function onCursorPlanChanged() { root.applySubscriptionPlan(cursorMonitor, plasmoid.configuration.cursorPlanId, plasmoid.configuration.cursorPlan, plasmoid.configuration.cursorCustomLimit); }
        function onCursorCustomLimitChanged() { root.applySubscriptionPlan(cursorMonitor, plasmoid.configuration.cursorPlanId, plasmoid.configuration.cursorPlan, plasmoid.configuration.cursorCustomLimit); }

        function onWindsurfPlanIdChanged() { root.applySubscriptionPlan(windsurfMonitor, plasmoid.configuration.windsurfPlanId, plasmoid.configuration.windsurfPlan, plasmoid.configuration.windsurfCustomLimit); }
        function onWindsurfPlanChanged() { root.applySubscriptionPlan(windsurfMonitor, plasmoid.configuration.windsurfPlanId, plasmoid.configuration.windsurfPlan, plasmoid.configuration.windsurfCustomLimit); }
        function onWindsurfCustomLimitChanged() { root.applySubscriptionPlan(windsurfMonitor, plasmoid.configuration.windsurfPlanId, plasmoid.configuration.windsurfPlan, plasmoid.configuration.windsurfCustomLimit); }

        function onJetbrainsAiPlanIdChanged() { root.applySubscriptionPlan(jetbrainsAiMonitor, plasmoid.configuration.jetbrainsAiPlanId, plasmoid.configuration.jetbrainsAiPlan, plasmoid.configuration.jetbrainsAiCustomLimit); }
        function onJetbrainsAiPlanChanged() { root.applySubscriptionPlan(jetbrainsAiMonitor, plasmoid.configuration.jetbrainsAiPlanId, plasmoid.configuration.jetbrainsAiPlan, plasmoid.configuration.jetbrainsAiCustomLimit); }
        function onJetbrainsAiCustomLimitChanged() { root.applySubscriptionPlan(jetbrainsAiMonitor, plasmoid.configuration.jetbrainsAiPlanId, plasmoid.configuration.jetbrainsAiPlan, plasmoid.configuration.jetbrainsAiCustomLimit); }
    }

    LocalMetricsServer {
        id: metricsServer
        enabled: plasmoid.configuration.prometheusEnabled
        port: plasmoid.configuration.prometheusPort
    }

    WebhookNotifier {
        id: webhookNotifier
        slackEnabled: plasmoid.configuration.slackWebhookEnabled
        discordEnabled: plasmoid.configuration.discordWebhookEnabled
        cooldownMinutes: plasmoid.configuration.webhookCooldownMinutes
    }

    ProviderRegistry {
        id: providerRegistry
        configuration: plasmoid.configuration

        openaiBackend: openaiBackend
        anthropicBackend: anthropicBackend
        googleBackend: googleBackend
        mistralBackend: mistralBackend
        deepseekBackend: deepseekBackend
        groqBackend: groqBackend
        xaiBackend: xaiBackend
        ollamaBackend: ollamaBackend
        openrouterBackend: openrouterBackend
        togetherBackend: togetherBackend
        cohereBackend: cohereBackend
        googleveoBackend: googleveoBackend
        azureBackend: azureBackend
        bedrockBackend: bedrockBackend
        providerManager: providerManager

        claudeCodeMonitor: claudeCodeMonitor
        codexCliMonitor: codexCliMonitor
        copilotMonitor: copilotMonitor
        cursorMonitor: cursorMonitor
        windsurfMonitor: windsurfMonitor
        jetbrainsAiMonitor: jetbrainsAiMonitor
    }

    NotificationController {
        id: notificationController
        configuration: plasmoid.configuration
        registry: providerRegistry
        usageDatabase: usageDatabase
        webhookNotifier: webhookNotifier
    }

    RefreshScheduler {
        id: refreshScheduler
        configuration: plasmoid.configuration
        registry: providerRegistry
        browserSyncService: browserSyncService
        claudeCodeMonitor: claudeCodeMonitor
        codexCliMonitor: codexCliMonitor
        copilotMonitor: copilotMonitor
        usageDatabase: usageDatabase
        popupOpen: !!plasmoid.expanded
    }

    RuntimeCoordinator {
        id: runtimeCoordinator
        configuration: plasmoid.configuration
        registry: providerRegistry
        secrets: secrets
        usageDatabase: usageDatabase
        notificationController: notificationController
        scheduler: refreshScheduler
        metricsServer: metricsServer
        webhookNotifier: webhookNotifier
        claudeCodeMonitor: claudeCodeMonitor
        codexCliMonitor: codexCliMonitor
        copilotMonitor: copilotMonitor
        cursorMonitor: cursorMonitor
        windsurfMonitor: windsurfMonitor
        jetbrainsAiMonitor: jetbrainsAiMonitor
    }

    UpdateChecker {
        id: updateChecker
        currentVersion: (plasmoid.metaData && plasmoid.metaData.version)
                        ? plasmoid.metaData.version
                        : AppInfo.version
        checkIntervalHours: plasmoid.configuration.updateCheckInterval || 12

        onUpdateAvailable: function(latestVersion, releaseUrl) {
            notificationController.sendUpdateAvailable(latestVersion, releaseUrl);
        }
    }

    IntelligenceEngine {
        id: analystIntelligence
    }

    compactRepresentation: CompactRepresentation {}
    fullRepresentation: FullRepresentation {}

    Component.onCompleted: {
        var saved = plasmoid.configuration.deepseekModel || "";
        var effective = ProviderPricingCatalog.effectiveModelId("deepseek", saved);
        if (saved && effective !== saved) {
            plasmoid.configuration.deepseekModel = effective;
            root.modelMigrationNotice = i18n("DeepSeek model %1 was retired and migrated to %2.", saved, effective);
        }
    }

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("Refresh All")
            icon.name: "view-refresh"
            onTriggered: root.refreshAll()
        },
        PlasmaCore.Action {
            text: i18n("Configure Settings")
            icon.name: "configure"
            onTriggered: plasmoid.internalAction("configure").trigger()
        },
        PlasmaCore.Action {
            text: i18n("Open Dashboard")
            icon.name: "window-new"
            onTriggered: plasmoid.expanded = true
        },
        PlasmaCore.Action {
            text: plasmoid.configuration.alertsEnabled ? i18n("Mute Alerts") : i18n("Unmute Alerts")
            icon.name: plasmoid.configuration.alertsEnabled ? "notifications-disabled" : "notifications"
            onTriggered: plasmoid.configuration.alertsEnabled = !plasmoid.configuration.alertsEnabled
        }
    ]
}
