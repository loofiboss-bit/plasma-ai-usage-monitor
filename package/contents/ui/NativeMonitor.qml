import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import com.github.loofi.aiusagemonitor 1.0
import "Utils.js" as Utils

Item {
    id: root
    property string modelMigrationNotice: ""
    property int pendingSettingsVerificationId: 0
    property string pendingSettingsVerificationSourceId: ""
    property bool diagnosticsSnapshotScheduled: false
    readonly property string pluginVersion: AppInfo.version
    readonly property string smokeView: AppInfo.smokeView
    readonly property bool pluginVersionMismatch: {
        var required = plasmoid.metaData?.version || "";
        return required !== "" && required !== AppInfo.version;
    }

    readonly property string toolTipSubText: {
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
    property alias sourceReadiness: sourceReadinessModel
    property alias secretsManager: secrets

    property alias claudeCode: claudeCodeMonitor
    property alias codexCli: codexCliMonitor
    property alias copilot: copilotMonitor
    property alias cursor: cursorMonitor
    property alias windsurf: windsurfMonitor
    property alias jetbrainsAi: jetbrainsAiMonitor
    property alias antigravity: antigravityMonitor
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

    function syncReadinessEnabled(stableId) {
        var provider = providerRegistry.providerByConfigKey(stableId);
        if (provider) {
            sourceReadinessModel.setSourceEnabled(stableId, provider.enabled);
            return;
        }
        var tools = providerRegistry.allSubscriptionTools || [];
        for (var i = 0; i < tools.length; i++) {
            if (tools[i].stableId === stableId) {
                sourceReadinessModel.setSourceEnabled(stableId, tools[i].enabled);
                return;
            }
        }
    }

    function configureSourceReadiness() {
        var providers = providerRegistry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            sourceReadinessModel.registerProviderBackend(providers[i].configKey, providers[i].backend);
            sourceReadinessModel.setSourceEnabled(providers[i].configKey, providers[i].enabled);
        }

        var localTools = [
            { stableId: "google-antigravity", backend: antigravityMonitor },
            { stableId: "claude-code", backend: claudeCodeMonitor },
            { stableId: "codex-cli", backend: codexCliMonitor },
            { stableId: "github-copilot", backend: copilotMonitor },
            { stableId: "cursor", backend: cursorMonitor },
            { stableId: "windsurf", backend: windsurfMonitor },
            { stableId: "jetbrains-ai", backend: jetbrainsAiMonitor }
        ];
        for (var j = 0; j < localTools.length; j++) {
            sourceReadinessModel.registerLocalTool(localTools[j].stableId, localTools[j].backend);
        }
    }

    function setGuidedSourceEnabled(stableId, enabled) {
        var provider = providerRegistry.providerByConfigKey(stableId);
        if (provider) {
            plasmoid.configuration[provider.enabledKey] = enabled;
            sourceReadinessModel.setSourceEnabled(stableId, enabled);
            return true;
        }

        var localConfigKeys = {
            "google-antigravity": "antigravityEnabled",
            "claude-code": "claudeCodeEnabled",
            "codex-cli": "codexEnabled",
            "github-copilot": "copilotEnabled",
            "cursor": "cursorEnabled",
            "windsurf": "windsurfEnabled",
            "jetbrains-ai": "jetbrainsAiEnabled"
        };
        var key = localConfigKeys[stableId];
        if (!key) return false;
        plasmoid.configuration[key] = enabled;
        return true;
    }

    function setGuidedSourceEndpoint(stableId, endpoint) {
        var provider = providerRegistry.providerByConfigKey(stableId);
        if (!provider || !provider.customBaseUrlKey) return endpoint.length === 0;
        plasmoid.configuration[provider.customBaseUrlKey] = endpoint.trim();
        if (provider.backend) provider.backend.customBaseUrl = endpoint.trim();
        return true;
    }

    function hasGuidedSourceEndpoint(stableId) {
        var provider = providerRegistry.providerByConfigKey(stableId);
        return !!(provider && provider.backend && provider.backend.customBaseUrl
                  && provider.backend.customBaseUrl.trim().length > 0);
    }

    function verifyGuidedSource(stableId) {
        return sourceReadinessModel.verifySource(stableId);
    }

    function fixOverviewSource(stableId, actionKey, sourceKindKey) {
        var retryActions = ["verify_source", "refresh_stale_data", "check_network", "retry_later"];
        if (retryActions.indexOf(actionKey) >= 0) {
            runtimeCoordinator.loadProviderApiKey(
                stableId, refreshScheduler.refreshCredentialChanged, false);
            if (sourceReadinessModel.verifySource(stableId)) return true;
        }

        plasmoid.configuration.settingsVerificationSourceId = stableId;
        var configureAction = plasmoid.internalAction("configure");
        if (configureAction) {
            configureAction.trigger();
            return true;
        }
        return false;
    }

    function settingsVerificationMessage(source) {
        var state = source.readinessStateKey || "failed";
        if (state === "reporting_actual")
            return i18n("Verification succeeded and returned supported usage or spend data.");
        if (state === "reporting_estimate")
            return i18n("Verification succeeded and returned estimated or local activity data.");
        if (state === "connected_connectivity_only")
            return i18n("Verification succeeded. This source confirms connectivity only.");
        if (state === "degraded")
            return source.nextActionText || i18n("Verification completed with degraded data.");
        return source.nextActionText || i18n("Verification failed. Review the source configuration and retry.");
    }

    function finishSettingsVerification(source) {
        if (!source || source.stableId !== pendingSettingsVerificationSourceId) return;
        var state = source.readinessStateKey || "failed";
        if (state === "verifying" || state === "ready_to_verify") return;
        plasmoid.configuration.settingsVerificationState = state;
        plasmoid.configuration.settingsVerificationMessage = root.settingsVerificationMessage(source);
        plasmoid.configuration.settingsVerificationTimestamp = new Date().toISOString();
        plasmoid.configuration.settingsVerificationCompletedRequestId = pendingSettingsVerificationId;
        pendingSettingsVerificationId = 0;
        pendingSettingsVerificationSourceId = "";
    }

    function processSettingsVerificationRequest() {
        var requestId = Number(plasmoid.configuration.settingsVerificationRequestId || 0);
        var completedId = Number(plasmoid.configuration.settingsVerificationCompletedRequestId || 0);
        var sourceId = plasmoid.configuration.settingsVerificationSourceId || "";
        if (requestId <= completedId || !sourceId) return;

        pendingSettingsVerificationId = requestId;
        pendingSettingsVerificationSourceId = sourceId;
        plasmoid.configuration.settingsVerificationState = "verifying";
        plasmoid.configuration.settingsVerificationMessage = i18n("Running the safe read-only scheduled check…");
        plasmoid.configuration.settingsVerificationTimestamp = "";

        Qt.callLater(function() {
            runtimeCoordinator.loadProviderApiKey(
                sourceId, refreshScheduler.refreshCredentialChanged, false);
            var started = sourceReadinessModel.verifySource(sourceId);
            if (!started) root.finishSettingsVerification(sourceReadinessModel.source(sourceId));
        });
    }

    function scheduleDiagnosticsSnapshot() {
        if (diagnosticsSnapshotScheduled) return;
        diagnosticsSnapshotScheduled = true;
        Qt.callLater(function() {
            diagnosticsSnapshotScheduled = false;
            var rows = [];
            var providers = providerRegistry.allProviders || [];
            var tools = providerRegistry.allSubscriptionTools || [];
            var ids = [];
            for (var i = 0; i < providers.length; i++) ids.push(providers[i].configKey);
            for (var j = 0; j < tools.length; j++) ids.push(tools[j].stableId);

            for (var k = 0; k < ids.length; k++) {
                var source = sourceReadinessModel.source(ids[k]);
                if (!source || !source.stableId) continue;
                rows.push({
                    stableId: source.stableId,
                    sourceKindKey: source.sourceKindKey,
                    enabled: !!source.enabled,
                    installed: !!source.installed,
                    readinessStateKey: source.readinessStateKey || "failed",
                    errorCode: source.errorCode || "",
                    nextActionKey: source.nextActionKey || "none",
                    lastVerifiedPresent: !!source.lastVerifiedPresent
                });
            }

            var snapshot = JSON.stringify(rows);
            if (plasmoid.configuration.diagnosticsSourceSnapshot !== snapshot)
                plasmoid.configuration.diagnosticsSourceSnapshot = snapshot;
        });
    }

    function refreshAll() {
        refreshScheduler.refreshAll();
    }

    function performBrowserSync() {
        refreshScheduler.performBrowserSync();
    }

    function refreshSubscriptionTool(descriptor) {
        if (!descriptor) return;
        if (descriptor.syncAction === "local" && descriptor.monitor) {
            descriptor.monitor.refreshQuota();
            return;
        }
        refreshScheduler.performBrowserSync();
    }

    function refreshSubscriptionTools() {
        refreshScheduler.performBrowserSync();
        refreshScheduler.refreshAntigravity(true);
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

    SourceReadinessModel {
        id: sourceReadinessModel

        onSourceChanged: function(stableId) {
            root.scheduleDiagnosticsSnapshot();
            if (stableId === root.pendingSettingsVerificationSourceId)
                root.finishSettingsVerification(sourceReadinessModel.source(stableId));
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

    AntigravityMonitor {
        id: antigravityMonitor
        enabled: plasmoid.configuration.antigravityEnabled
        warningThreshold: plasmoid.configuration.warningThreshold
        criticalThreshold: plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            if (enabled) Qt.callLater(refreshQuota);
        }
    }

    Connections {
        target: plasmoid.configuration

        function onOpenaiEnabledChanged() { root.syncReadinessEnabled("openai"); }
        function onAnthropicEnabledChanged() { root.syncReadinessEnabled("anthropic"); }
        function onGoogleEnabledChanged() { root.syncReadinessEnabled("google"); }
        function onMistralEnabledChanged() { root.syncReadinessEnabled("mistral"); }
        function onDeepseekEnabledChanged() { root.syncReadinessEnabled("deepseek"); }
        function onGroqEnabledChanged() { root.syncReadinessEnabled("groq"); }
        function onXaiEnabledChanged() { root.syncReadinessEnabled("xai"); }
        function onOllamaEnabledChanged() { root.syncReadinessEnabled("ollama"); }
        function onOpenrouterEnabledChanged() { root.syncReadinessEnabled("openrouter"); }
        function onTogetherEnabledChanged() { root.syncReadinessEnabled("together"); }
        function onCohereEnabledChanged() { root.syncReadinessEnabled("cohere"); }
        function onGoogleveoEnabledChanged() { root.syncReadinessEnabled("googleveo"); }
        function onAzureEnabledChanged() { root.syncReadinessEnabled("azure"); }
        function onBedrockEnabledChanged() { root.syncReadinessEnabled("bedrock"); }
        function onLitellmEnabledChanged() { root.syncReadinessEnabled("litellm"); }
        function onCerebrasEnabledChanged() { root.syncReadinessEnabled("cerebras"); }
        function onFireworksEnabledChanged() { root.syncReadinessEnabled("fireworks"); }
        function onPerplexityEnabledChanged() { root.syncReadinessEnabled("perplexity"); }
        function onSettingsVerificationRequestIdChanged() { root.processSettingsVerificationRequest(); }
        function onAntigravityEnabledChanged() {
            root.syncReadinessEnabled("google-antigravity");
            if (plasmoid.configuration.antigravityEnabled) antigravityMonitor.refreshQuota();
        }

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
        antigravityMonitor: antigravityMonitor
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
        antigravityMonitor: antigravityMonitor
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
        antigravityMonitor: antigravityMonitor
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

    property Component compactRepresentationComponent: CompactRepresentation {}
    property Component fullRepresentationComponent: FullRepresentation {}

    Component.onCompleted: {
        root.configureSourceReadiness();
        root.scheduleDiagnosticsSnapshot();
        root.processSettingsVerificationRequest();
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
