pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import com.github.loofi.aiusagemonitor 1.0
import "Utils.js" as Utils
import "components" as Components

Item {
    id: root
    property string modelMigrationNotice: ""
    property int pendingSettingsVerificationId: 0
    property string pendingSettingsVerificationSourceId: ""
    property bool diagnosticsSnapshotScheduled: false
    readonly property string pluginVersion: AppInfo.version
    readonly property string smokeView: AppInfo.smokeView
    // Plasma's KPluginMetaData value type is absent from its installed qmltypes.
    // qmllint disable unresolved-type
    readonly property bool pluginVersionMismatch: {
        var required = Plasmoid["metaData"]?.["version"] || "";
        return required !== "" && required !== AppInfo.version;
    }
    // qmllint enable unresolved-type

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
    property alias dailyState: dailyStateModel
    property alias guardrails: guardrailModel
    property alias budgetPolicies: budgetPolicyRepository
    readonly property var presentationDailyState: mediaDailyState.active
        ? mediaDailyState : dailyStateModel
    property alias secretsManager: secrets

    property alias claudeCode: claudeCodeMonitor
    property alias codexCli: codexCliMonitor
    property alias copilot: copilotMonitor
    property alias cursor: cursorMonitor
    property alias windsurf: windsurfMonitor
    property alias jetbrainsAi: jetbrainsAiMonitor
    property alias antigravity: antigravityMonitor
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

    function setGuidedSourceEnabled(stableId, enabled) {
        var provider = providerRegistry.providerByConfigKey(stableId);
        if (provider) {
            Plasmoid.configuration[provider.enabledKey] = enabled;
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
        Plasmoid.configuration[key] = enabled;
        return true;
    }

    function setGuidedSourceEndpoint(stableId, endpoint) {
        var provider = providerRegistry.providerByConfigKey(stableId);
        if (!provider || !provider.customBaseUrlKey) return endpoint.length === 0;
        Plasmoid.configuration[provider.customBaseUrlKey] = endpoint.trim();
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

        Plasmoid.configuration.settingsVerificationSourceId = stableId;
        var configureAction = Plasmoid.internalAction("configure");
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
        Plasmoid.configuration.settingsVerificationState = state;
        Plasmoid.configuration.settingsVerificationMessage = root.settingsVerificationMessage(source);
        Plasmoid.configuration.settingsVerificationTimestamp = new Date().toISOString();
        Plasmoid.configuration.settingsVerificationCompletedRequestId = pendingSettingsVerificationId;
        pendingSettingsVerificationId = 0;
        pendingSettingsVerificationSourceId = "";
    }

    function processSettingsVerificationRequest() {
        var requestId = Number(Plasmoid.configuration.settingsVerificationRequestId || 0);
        var completedId = Number(Plasmoid.configuration.settingsVerificationCompletedRequestId || 0);
        var sourceId = Plasmoid.configuration.settingsVerificationSourceId || "";
        if (requestId <= completedId || !sourceId) return;

        pendingSettingsVerificationId = requestId;
        pendingSettingsVerificationSourceId = sourceId;
        Plasmoid.configuration.settingsVerificationState = "verifying";
        Plasmoid.configuration.settingsVerificationMessage = i18n("Running the safe read-only scheduled check…");
        Plasmoid.configuration.settingsVerificationTimestamp = "";

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
            if (Plasmoid.configuration.diagnosticsSourceSnapshot !== snapshot)
                Plasmoid.configuration.diagnosticsSourceSnapshot = snapshot;
        });
    }

    function refreshAll() {
        refreshScheduler.refreshAll();
    }

    function refreshGuardrails() {
        runtimeCoordinator.scheduleGuardrailRefresh();
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

    function migrateLegacyBudgets() {
        var rows = [];
        var providers = policyCatalog.budgetProviders || [];
        var warning = Number(Plasmoid.configuration.budgetWarningPercent || 80);
        for (var i = 0; i < providers.length; ++i) {
            var provider = providers[i];
            var periods = [
                { key: provider.dailyBudgetConfigKey, type: "calendar_day" },
                { key: provider.monthlyBudgetConfigKey, type: "calendar_month" }
            ];
            for (var periodIndex = 0; periodIndex < periods.length; ++periodIndex) {
                var period = periods[periodIndex];
                if (!period.key) continue;
                rows.push({
                    legacyKey: period.key,
                    sourceId: provider.configKey,
                    sourceKind: "provider",
                    scopeMode: "aggregate",
                    scopeKind: "",
                    scopeIdentity: "",
                    scopeLabel: "",
                    valueClass: "actual",
                    limitMinor: Number(Plasmoid.configuration[period.key] || 0),
                    currency: "USD",
                    periodType: period.type,
                    timeZoneId: "UTC",
                    warningPercent: warning,
                    notifyEnabled: true,
                    enabled: true
                });
            }
        }
        return budgetPolicyRepository.migrateLegacyBudgets(rows);
    }

    SecretsManager {
        id: secrets
    }

    UsageDatabase {
        id: usageDatabase
        enabled: Plasmoid.configuration.historyEnabled
            || Plasmoid.configuration.forecastNotificationsEnabled
            || (AppInfo.demoMode
                && (AppInfo.smokeView.indexOf("media-history") === 0
                    || AppInfo.smokeView.indexOf("media-analyst") === 0
                    || AppInfo.smokeView === "media-source-detail"))
        retentionDays: Plasmoid.configuration.historyRetentionDays
    }

    ProviderCatalog { id: policyCatalog }

    BudgetPolicyRepository {
        id: budgetPolicyRepository
        // Plasma's attached qmltypes omit the stable applet instance id.
        // qmllint disable unresolved-type
        ownerId: "applet:" + String(Plasmoid["id"])
        // qmllint enable unresolved-type
    }

    OpenAIProvider {
        id: openaiBackend
        model: Plasmoid.configuration.openaiModel
        projectId: Plasmoid.configuration.openaiProjectId
        customBaseUrl: Plasmoid.configuration.openaiCustomBaseUrl
    }

    AzureOpenAIProvider {
        id: azureBackend
        model: Plasmoid.configuration.azureModel
        deploymentId: Plasmoid.configuration.azureDeploymentId
        customBaseUrl: Plasmoid.configuration.azureCustomBaseUrl
    }

    BedrockProvider {
        id: bedrockBackend
        region: Plasmoid.configuration.bedrockRegion
        model: Plasmoid.configuration.bedrockModel
        customBaseUrl: Plasmoid.configuration.bedrockCustomBaseUrl
    }

    AnthropicProvider {
        id: anthropicBackend
        model: Plasmoid.configuration.anthropicModel
        customBaseUrl: Plasmoid.configuration.anthropicCustomBaseUrl
    }

    GoogleProvider {
        id: googleBackend
        model: Plasmoid.configuration.googleModel
        tier: Plasmoid.configuration.googleTier
        customBaseUrl: Plasmoid.configuration.googleCustomBaseUrl
    }

    MistralProvider {
        id: mistralBackend
        model: Plasmoid.configuration.mistralModel
        customBaseUrl: Plasmoid.configuration.mistralCustomBaseUrl
    }

    DeepSeekProvider {
        id: deepseekBackend
        model: Plasmoid.configuration.deepseekModel
        customBaseUrl: Plasmoid.configuration.deepseekCustomBaseUrl
    }

    GroqProvider {
        id: groqBackend
        model: Plasmoid.configuration.groqModel
        customBaseUrl: Plasmoid.configuration.groqCustomBaseUrl
    }

    XAIProvider {
        id: xaiBackend
        model: Plasmoid.configuration.xaiModel
        customBaseUrl: Plasmoid.configuration.xaiCustomBaseUrl
    }

    OllamaCloudProvider {
        id: ollamaBackend
        model: Plasmoid.configuration.ollamaModel
        customBaseUrl: Plasmoid.configuration.ollamaCustomBaseUrl
    }

    OpenRouterProvider {
        id: openrouterBackend
        model: Plasmoid.configuration.openrouterModel
        customBaseUrl: Plasmoid.configuration.openrouterCustomBaseUrl
    }

    TogetherProvider {
        id: togetherBackend
        model: Plasmoid.configuration.togetherModel
        customBaseUrl: Plasmoid.configuration.togetherCustomBaseUrl
    }

    CohereProvider {
        id: cohereBackend
        model: Plasmoid.configuration.cohereModel
        customBaseUrl: Plasmoid.configuration.cohereCustomBaseUrl
    }

    GoogleVeoProvider {
        id: googleveoBackend
        model: Plasmoid.configuration.googleveoModel
        tier: Plasmoid.configuration.googleveoTier
        customBaseUrl: Plasmoid.configuration.googleveoCustomBaseUrl
    }

    ProviderManager {
        id: providerManager
    }

    ProviderRuntimeRegistration {
        id: providerRuntimeRegistration
        configuration: Plasmoid.configuration
        providerManager: providerManager
        providerRegistry: providerRegistry
        sourceReadinessModel: sourceReadinessModel
        dailyStateModel: dailyStateModel
        nativeBackends: [
            { configKey: "openai", backend: openaiBackend },
            { configKey: "anthropic", backend: anthropicBackend },
            { configKey: "google", backend: googleBackend },
            { configKey: "mistral", backend: mistralBackend },
            { configKey: "deepseek", backend: deepseekBackend },
            { configKey: "groq", backend: groqBackend },
            { configKey: "xai", backend: xaiBackend },
            { configKey: "ollama", backend: ollamaBackend },
            { configKey: "openrouter", backend: openrouterBackend },
            { configKey: "together", backend: togetherBackend },
            { configKey: "cohere", backend: cohereBackend },
            { configKey: "googleveo", backend: googleveoBackend },
            { configKey: "azure", backend: azureBackend },
            { configKey: "bedrock", backend: bedrockBackend }
        ]
    }

    SourceReadinessModel {
        id: sourceReadinessModel

        onSourceChanged: function(stableId) {
            root.scheduleDiagnosticsSnapshot();
            if (stableId === root.pendingSettingsVerificationSourceId)
                root.finishSettingsVerification(sourceReadinessModel.source(stableId));
        }
    }

    DailyStateModel {
        id: dailyStateModel
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold
    }

    GuardrailModel {
        id: guardrailModel
    }

    Components.MediaDailyState {
        id: mediaDailyState
    }

    BrowserSyncService {
        id: browserSyncService
        browserType: Plasmoid.configuration.browserSyncBrowser
        selectedFirefoxProfile: Plasmoid.configuration.browserSyncProfile
    }

    ClaudeCodeMonitor {
        id: claudeCodeMonitor
        enabled: Plasmoid.configuration.claudeCodeEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            syncEnabled = Qt.binding(function() { return Plasmoid.configuration.browserSyncEnabled; });
            root.applySubscriptionPlan(claudeCodeMonitor, Plasmoid.configuration.claudeCodePlanId, Plasmoid.configuration.claudeCodePlan, Plasmoid.configuration.claudeCodeCustomLimit);
        }
    }

    CodexCliMonitor {
        id: codexCliMonitor
        enabled: Plasmoid.configuration.codexEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            syncEnabled = Qt.binding(function() { return Plasmoid.configuration.browserSyncEnabled; });
            root.applySubscriptionPlan(codexCliMonitor, Plasmoid.configuration.codexPlanId, Plasmoid.configuration.codexPlan, Plasmoid.configuration.codexCustomLimit);
        }
    }

    CopilotMonitor {
        id: copilotMonitor
        enabled: Plasmoid.configuration.copilotEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold
        orgName: Plasmoid.configuration.copilotOrgName
        billingMode: Plasmoid.configuration.copilotBillingMode || "auto"
        monthlyResetDay: Plasmoid.configuration.copilotResetDay || 1

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(copilotMonitor, Plasmoid.configuration.copilotPlanId, Plasmoid.configuration.copilotPlan, Plasmoid.configuration.copilotCustomLimit);
            fetchOrgMetrics();
        }
    }

    CursorMonitor {
        id: cursorMonitor
        enabled: Plasmoid.configuration.cursorEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(cursorMonitor, Plasmoid.configuration.cursorPlanId, Plasmoid.configuration.cursorPlan, Plasmoid.configuration.cursorCustomLimit);
        }
    }

    WindsurfMonitor {
        id: windsurfMonitor
        enabled: Plasmoid.configuration.windsurfEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(windsurfMonitor, Plasmoid.configuration.windsurfPlanId, Plasmoid.configuration.windsurfPlan, Plasmoid.configuration.windsurfCustomLimit);
        }
    }

    JetBrainsAiMonitor {
        id: jetbrainsAiMonitor
        enabled: Plasmoid.configuration.jetbrainsAiEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            root.applySubscriptionPlan(jetbrainsAiMonitor, Plasmoid.configuration.jetbrainsAiPlanId, Plasmoid.configuration.jetbrainsAiPlan, Plasmoid.configuration.jetbrainsAiCustomLimit);
        }
    }

    AntigravityMonitor {
        id: antigravityMonitor
        enabled: Plasmoid.configuration.antigravityEnabled
        warningThreshold: Plasmoid.configuration.warningThreshold
        criticalThreshold: Plasmoid.configuration.criticalThreshold

        Component.onCompleted: {
            checkToolInstalled();
            if (enabled) Qt.callLater(refreshQuota);
        }
    }

    Connections {
        target: Plasmoid.configuration

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
            if (Plasmoid.configuration.antigravityEnabled) antigravityMonitor.refreshQuota();
        }

        function onClaudeCodePlanIdChanged() { root.applySubscriptionPlan(claudeCodeMonitor, Plasmoid.configuration.claudeCodePlanId, Plasmoid.configuration.claudeCodePlan, Plasmoid.configuration.claudeCodeCustomLimit); }
        function onClaudeCodePlanChanged() { root.applySubscriptionPlan(claudeCodeMonitor, Plasmoid.configuration.claudeCodePlanId, Plasmoid.configuration.claudeCodePlan, Plasmoid.configuration.claudeCodeCustomLimit); }
        function onClaudeCodeCustomLimitChanged() { root.applySubscriptionPlan(claudeCodeMonitor, Plasmoid.configuration.claudeCodePlanId, Plasmoid.configuration.claudeCodePlan, Plasmoid.configuration.claudeCodeCustomLimit); }

        function onCodexPlanIdChanged() { root.applySubscriptionPlan(codexCliMonitor, Plasmoid.configuration.codexPlanId, Plasmoid.configuration.codexPlan, Plasmoid.configuration.codexCustomLimit); }
        function onCodexPlanChanged() { root.applySubscriptionPlan(codexCliMonitor, Plasmoid.configuration.codexPlanId, Plasmoid.configuration.codexPlan, Plasmoid.configuration.codexCustomLimit); }
        function onCodexCustomLimitChanged() { root.applySubscriptionPlan(codexCliMonitor, Plasmoid.configuration.codexPlanId, Plasmoid.configuration.codexPlan, Plasmoid.configuration.codexCustomLimit); }

        function onCopilotPlanIdChanged() { root.applySubscriptionPlan(copilotMonitor, Plasmoid.configuration.copilotPlanId, Plasmoid.configuration.copilotPlan, Plasmoid.configuration.copilotCustomLimit); }
        function onCopilotPlanChanged() { root.applySubscriptionPlan(copilotMonitor, Plasmoid.configuration.copilotPlanId, Plasmoid.configuration.copilotPlan, Plasmoid.configuration.copilotCustomLimit); }
        function onCopilotCustomLimitChanged() { root.applySubscriptionPlan(copilotMonitor, Plasmoid.configuration.copilotPlanId, Plasmoid.configuration.copilotPlan, Plasmoid.configuration.copilotCustomLimit); }

        function onCursorPlanIdChanged() { root.applySubscriptionPlan(cursorMonitor, Plasmoid.configuration.cursorPlanId, Plasmoid.configuration.cursorPlan, Plasmoid.configuration.cursorCustomLimit); }
        function onCursorPlanChanged() { root.applySubscriptionPlan(cursorMonitor, Plasmoid.configuration.cursorPlanId, Plasmoid.configuration.cursorPlan, Plasmoid.configuration.cursorCustomLimit); }
        function onCursorCustomLimitChanged() { root.applySubscriptionPlan(cursorMonitor, Plasmoid.configuration.cursorPlanId, Plasmoid.configuration.cursorPlan, Plasmoid.configuration.cursorCustomLimit); }

        function onWindsurfPlanIdChanged() { root.applySubscriptionPlan(windsurfMonitor, Plasmoid.configuration.windsurfPlanId, Plasmoid.configuration.windsurfPlan, Plasmoid.configuration.windsurfCustomLimit); }
        function onWindsurfPlanChanged() { root.applySubscriptionPlan(windsurfMonitor, Plasmoid.configuration.windsurfPlanId, Plasmoid.configuration.windsurfPlan, Plasmoid.configuration.windsurfCustomLimit); }
        function onWindsurfCustomLimitChanged() { root.applySubscriptionPlan(windsurfMonitor, Plasmoid.configuration.windsurfPlanId, Plasmoid.configuration.windsurfPlan, Plasmoid.configuration.windsurfCustomLimit); }

        function onJetbrainsAiPlanIdChanged() { root.applySubscriptionPlan(jetbrainsAiMonitor, Plasmoid.configuration.jetbrainsAiPlanId, Plasmoid.configuration.jetbrainsAiPlan, Plasmoid.configuration.jetbrainsAiCustomLimit); }
        function onJetbrainsAiPlanChanged() { root.applySubscriptionPlan(jetbrainsAiMonitor, Plasmoid.configuration.jetbrainsAiPlanId, Plasmoid.configuration.jetbrainsAiPlan, Plasmoid.configuration.jetbrainsAiCustomLimit); }
        function onJetbrainsAiCustomLimitChanged() { root.applySubscriptionPlan(jetbrainsAiMonitor, Plasmoid.configuration.jetbrainsAiPlanId, Plasmoid.configuration.jetbrainsAiPlan, Plasmoid.configuration.jetbrainsAiCustomLimit); }
    }

    LocalMetricsServer {
        id: metricsServer
        enabled: Plasmoid.configuration.prometheusEnabled
        port: Plasmoid.configuration.prometheusPort
    }

    WebhookNotifier {
        id: webhookNotifier
        slackEnabled: Plasmoid.configuration.slackWebhookEnabled
        discordEnabled: Plasmoid.configuration.discordWebhookEnabled
        cooldownMinutes: Plasmoid.configuration.webhookCooldownMinutes
    }

    ProviderRegistry {
        id: providerRegistry
        configuration: Plasmoid.configuration

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
        configuration: Plasmoid.configuration
        registry: providerRegistry
        dailyState: dailyStateModel
        guardrails: guardrailModel
        usageDatabase: usageDatabase
        webhookNotifier: webhookNotifier
    }

    RefreshScheduler {
        id: refreshScheduler
        configuration: Plasmoid.configuration
        registry: providerRegistry
        browserSyncService: browserSyncService
        claudeCodeMonitor: claudeCodeMonitor
        codexCliMonitor: codexCliMonitor
        copilotMonitor: copilotMonitor
        antigravityMonitor: antigravityMonitor
        usageDatabase: usageDatabase
        // Plasma's attached qmltypes omit the runtime expanded member.
        // qmllint disable missing-property
        popupOpen: !!Plasmoid["expanded"]
        // qmllint enable missing-property
    }

    RuntimeCoordinator {
        id: runtimeCoordinator
        configuration: Plasmoid.configuration
        registry: providerRegistry
        secrets: secrets
        usageDatabase: usageDatabase
        guardrailModel: guardrailModel
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
        // Plasma's KPluginMetaData value type is absent from its installed qmltypes.
        // qmllint disable unresolved-type
        currentVersion: (Plasmoid["metaData"]
                         && Plasmoid["metaData"]["version"])
                        ? Plasmoid["metaData"]["version"]
                        : AppInfo.version
        // qmllint enable unresolved-type
        checkIntervalHours: Plasmoid.configuration.updateCheckInterval || 12

        onUpdateAvailable: function(latestVersion, releaseUrl) {
            notificationController.sendUpdateAvailable(latestVersion, releaseUrl);
        }
    }

    property Component compactRepresentationComponent: CompactRepresentation {
        // Component boundaries intentionally capture their owning monitor.
        // qmllint disable unqualified
        monitor: root
        // qmllint enable unqualified
    }
    property Component fullRepresentationComponent: FullRepresentation {
        // Component boundaries intentionally capture their owning monitor.
        // qmllint disable unqualified
        monitor: root
        // qmllint enable unqualified
    }

    Component.onCompleted: {
        usageDatabase.init();
        if (!budgetPolicyRepository.init() || !root.migrateLegacyBudgets())
            console.error("Budget policy initialization failed:", budgetPolicyRepository.errorString);
        providerRuntimeRegistration.initialize();
        root.scheduleDiagnosticsSnapshot();
        root.processSettingsVerificationRequest();
        var saved = Plasmoid.configuration.deepseekModel || "";
        var effective = ProviderPricingCatalog.effectiveModelId("deepseek", saved);
        if (saved && effective !== saved) {
            Plasmoid.configuration.deepseekModel = effective;
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
            onTriggered: Plasmoid.internalAction("configure").trigger()
        },
        PlasmaCore.Action {
            text: i18n("Open Dashboard")
            icon.name: "window-new"
            onTriggered: Plasmoid.expanded = true
        },
        PlasmaCore.Action {
            text: Plasmoid.configuration.alertsEnabled ? i18n("Mute Alerts") : i18n("Unmute Alerts")
            icon.name: Plasmoid.configuration.alertsEnabled ? "notifications-disabled" : "notifications"
            onTriggered: Plasmoid.configuration.alertsEnabled = !Plasmoid.configuration.alertsEnabled
        }
    ]
}
