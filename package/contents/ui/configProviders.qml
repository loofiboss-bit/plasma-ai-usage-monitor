import QtQuick
import QtQuick.Window
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0
import "components" as Components

KCM.SimpleKCM {
    id: providersPage

    signal configurationChanged()


    property bool cfg_openaiEnabled: Plasmoid.configuration.openaiEnabled
    property string cfg_openaiModel: Plasmoid.configuration.openaiModel
    property string cfg_openaiProjectId: Plasmoid.configuration.openaiProjectId
    property string cfg_openaiCustomBaseUrl: Plasmoid.configuration.openaiCustomBaseUrl
    property bool cfg_anthropicEnabled: Plasmoid.configuration.anthropicEnabled
    property string cfg_anthropicModel: Plasmoid.configuration.anthropicModel
    property string cfg_anthropicCustomBaseUrl: Plasmoid.configuration.anthropicCustomBaseUrl
    property bool cfg_googleEnabled: Plasmoid.configuration.googleEnabled
    property string cfg_googleModel: Plasmoid.configuration.googleModel
    property string cfg_googleTier: Plasmoid.configuration.googleTier
    property string cfg_googleCustomBaseUrl: Plasmoid.configuration.googleCustomBaseUrl
    property bool cfg_mistralEnabled: Plasmoid.configuration.mistralEnabled
    property string cfg_mistralModel: Plasmoid.configuration.mistralModel
    property string cfg_mistralCustomBaseUrl: Plasmoid.configuration.mistralCustomBaseUrl
    property bool cfg_deepseekEnabled: Plasmoid.configuration.deepseekEnabled
    property string cfg_deepseekModel: Plasmoid.configuration.deepseekModel
    property string cfg_deepseekCustomBaseUrl: Plasmoid.configuration.deepseekCustomBaseUrl
    property bool cfg_groqEnabled: Plasmoid.configuration.groqEnabled
    property string cfg_groqModel: Plasmoid.configuration.groqModel
    property string cfg_groqCustomBaseUrl: Plasmoid.configuration.groqCustomBaseUrl
    property bool cfg_xaiEnabled: Plasmoid.configuration.xaiEnabled
    property string cfg_xaiModel: Plasmoid.configuration.xaiModel
    property string cfg_xaiCustomBaseUrl: Plasmoid.configuration.xaiCustomBaseUrl
    property bool cfg_ollamaEnabled: Plasmoid.configuration.ollamaEnabled
    property string cfg_ollamaModel: Plasmoid.configuration.ollamaModel
    property string cfg_ollamaCustomBaseUrl: Plasmoid.configuration.ollamaCustomBaseUrl
    property bool cfg_openrouterEnabled: Plasmoid.configuration.openrouterEnabled
    property string cfg_openrouterModel: Plasmoid.configuration.openrouterModel
    property string cfg_openrouterCustomBaseUrl: Plasmoid.configuration.openrouterCustomBaseUrl
    property bool cfg_togetherEnabled: Plasmoid.configuration.togetherEnabled
    property string cfg_togetherModel: Plasmoid.configuration.togetherModel
    property string cfg_togetherCustomBaseUrl: Plasmoid.configuration.togetherCustomBaseUrl
    property bool cfg_cohereEnabled: Plasmoid.configuration.cohereEnabled
    property string cfg_cohereModel: Plasmoid.configuration.cohereModel
    property string cfg_cohereCustomBaseUrl: Plasmoid.configuration.cohereCustomBaseUrl
    property bool cfg_googleveoEnabled: Plasmoid.configuration.googleveoEnabled
    property string cfg_googleveoModel: Plasmoid.configuration.googleveoModel
    property string cfg_googleveoTier: Plasmoid.configuration.googleveoTier
    property string cfg_googleveoCustomBaseUrl: Plasmoid.configuration.googleveoCustomBaseUrl
    property bool cfg_azureEnabled: Plasmoid.configuration.azureEnabled
    property string cfg_azureModel: Plasmoid.configuration.azureModel
    property string cfg_azureDeploymentId: Plasmoid.configuration.azureDeploymentId
    property string cfg_azureCustomBaseUrl: Plasmoid.configuration.azureCustomBaseUrl
    property bool cfg_bedrockEnabled: Plasmoid.configuration.bedrockEnabled
    property string cfg_bedrockRegion: Plasmoid.configuration.bedrockRegion
    property string cfg_bedrockModel: Plasmoid.configuration.bedrockModel
    property string cfg_bedrockCustomBaseUrl: Plasmoid.configuration.bedrockCustomBaseUrl
    property bool cfg_litellmEnabled: Plasmoid.configuration.litellmEnabled
    property string cfg_litellmModel: Plasmoid.configuration.litellmModel
    property string cfg_litellmCustomBaseUrl: Plasmoid.configuration.litellmCustomBaseUrl
    property bool cfg_cerebrasEnabled: Plasmoid.configuration.cerebrasEnabled
    property string cfg_cerebrasModel: Plasmoid.configuration.cerebrasModel
    property string cfg_cerebrasCustomBaseUrl: Plasmoid.configuration.cerebrasCustomBaseUrl
    property bool cfg_fireworksEnabled: Plasmoid.configuration.fireworksEnabled
    property string cfg_fireworksModel: Plasmoid.configuration.fireworksModel
    property string cfg_fireworksCustomBaseUrl: Plasmoid.configuration.fireworksCustomBaseUrl
    property bool cfg_perplexityEnabled: Plasmoid.configuration.perplexityEnabled
    property string cfg_perplexityModel: Plasmoid.configuration.perplexityModel
    property string cfg_perplexityCustomBaseUrl: Plasmoid.configuration.perplexityCustomBaseUrl
    property bool cfg_claudeCodeEnabled: Plasmoid.configuration.claudeCodeEnabled
    property bool cfg_codexEnabled: Plasmoid.configuration.codexEnabled
    property bool cfg_copilotEnabled: Plasmoid.configuration.copilotEnabled
    property bool cfg_cursorEnabled: Plasmoid.configuration.cursorEnabled
    property bool cfg_windsurfEnabled: Plasmoid.configuration.windsurfEnabled
    property bool cfg_jetbrainsAiEnabled: Plasmoid.configuration.jetbrainsAiEnabled
    property bool cfg_antigravityEnabled: Plasmoid.configuration.antigravityEnabled
    property bool cfg_antigravityNotifications: Plasmoid.configuration.antigravityNotifications
    property int cfg_antigravityRefreshInterval: Plasmoid.configuration.antigravityRefreshInterval || 300
    property bool cfg_claudeCodeNotifications: Plasmoid.configuration.claudeCodeNotifications
    property string cfg_claudeCodePlanId: Plasmoid.configuration.claudeCodePlanId || "pro"
    property int cfg_claudeCodePlan: Plasmoid.configuration.claudeCodePlan || 0
    property int cfg_claudeCodeCustomLimit: Plasmoid.configuration.claudeCodeCustomLimit
    property bool cfg_codexNotifications: Plasmoid.configuration.codexNotifications
    property string cfg_codexPlanId: Plasmoid.configuration.codexPlanId || "plus"
    property int cfg_codexPlan: Plasmoid.configuration.codexPlan || 0
    property int cfg_codexCustomLimit: Plasmoid.configuration.codexCustomLimit
    property bool cfg_copilotNotifications: Plasmoid.configuration.copilotNotifications
    property string cfg_copilotPlanId: Plasmoid.configuration.copilotPlanId || "free"
    property int cfg_copilotPlan: Plasmoid.configuration.copilotPlan || 0
    property int cfg_copilotCustomLimit: Plasmoid.configuration.copilotCustomLimit
    property string cfg_copilotBillingMode: Plasmoid.configuration.copilotBillingMode || "auto"
    property int cfg_copilotResetDay: Plasmoid.configuration.copilotResetDay || 1
    property string cfg_copilotOrgName: Plasmoid.configuration.copilotOrgName || ""
    property bool cfg_cursorNotifications: Plasmoid.configuration.cursorNotifications
    property string cfg_cursorPlanId: Plasmoid.configuration.cursorPlanId || "pro"
    property int cfg_cursorPlan: Plasmoid.configuration.cursorPlan || 0
    property int cfg_cursorCustomLimit: Plasmoid.configuration.cursorCustomLimit
    property bool cfg_windsurfNotifications: Plasmoid.configuration.windsurfNotifications
    property string cfg_windsurfPlanId: Plasmoid.configuration.windsurfPlanId || "pro"
    property int cfg_windsurfPlan: Plasmoid.configuration.windsurfPlan || 0
    property int cfg_windsurfCustomLimit: Plasmoid.configuration.windsurfCustomLimit
    property bool cfg_jetbrainsAiNotifications: Plasmoid.configuration.jetbrainsAiNotifications
    property string cfg_jetbrainsAiPlanId: Plasmoid.configuration.jetbrainsAiPlanId || "ai_free"
    property int cfg_jetbrainsAiPlan: Plasmoid.configuration.jetbrainsAiPlan || 0
    property int cfg_jetbrainsAiCustomLimit: Plasmoid.configuration.jetbrainsAiCustomLimit
    property bool cfg_browserSyncEnabled: Plasmoid.configuration.browserSyncEnabled
    property int cfg_browserSyncBrowser: Plasmoid.configuration.browserSyncBrowser
    property int cfg_browserSyncInterval: Plasmoid.configuration.browserSyncInterval || 300
    property string cfg_browserSyncProfile: Plasmoid.configuration.browserSyncProfile || ""

    readonly property bool advancedMode: Plasmoid.configuration.advancedSettingsMode
    readonly property bool walletOpen: secrets.walletOpen
    readonly property bool hasUnsavedChanges: settingsController.dirty || secretChanges.dirty
    readonly property bool unsavedChanges: secretChanges.dirty
    readonly property string verificationSourceId: Plasmoid.configuration.settingsVerificationSourceId || ""
    readonly property string verificationState: Plasmoid.configuration.settingsVerificationState || ""
    readonly property string verificationMessage: Plasmoid.configuration.settingsVerificationMessage || ""
    readonly property string verificationTimestamp: Plasmoid.configuration.settingsVerificationTimestamp || ""
    property bool retainDraftOnDeparture: false
    property bool showSourceDetails: false
    property int secretCommitRevision: 0
    property var browserTestResults: ({})
    readonly property bool compactSources: width < Kirigami.Units.gridUnit * 40

    Components.ConfigApplyGuard {
        id: applyGuard
        window: providersPage.Window.window
        onRetryReady: {
            providersPage.retainDraftOnDeparture = false;
            providersPage.configurationChanged();
        }
    }

    property string secretStatusMessage: ""
    property bool secretStatusError: false

    ProviderCatalog { id: descriptorCatalog }

    ClaudeCodeMonitor { id: claudeDetector; Component.onCompleted: checkToolInstalled() }
    CodexCliMonitor { id: codexDetector; Component.onCompleted: checkToolInstalled() }
    CopilotMonitor { id: copilotDetector; Component.onCompleted: checkToolInstalled() }
    CursorMonitor { id: cursorDetector; Component.onCompleted: checkToolInstalled() }
    WindsurfMonitor { id: windsurfDetector; Component.onCompleted: checkToolInstalled() }
    JetBrainsAiMonitor { id: jetbrainsDetector; Component.onCompleted: checkToolInstalled() }
    AntigravityMonitor { id: antigravityDetector; Component.onCompleted: checkToolInstalled() }

    readonly property var detectedLocalSources: {
        var candidates = [
            { key: "google-antigravity", name: "Google Antigravity", enabled: "antigravityEnabled", monitor: antigravityDetector,
              monitoringLevel: "actual_quota", notifications: "antigravityNotifications", refreshInterval: "antigravityRefreshInterval" },
            { key: "claude-code", name: "Claude Code", enabled: "claudeCodeEnabled", monitor: claudeDetector, plan: "claudeCodePlanId", legacyPlan: "claudeCodePlan", limit: "claudeCodeCustomLimit", notifications: "claudeCodeNotifications" },
            { key: "codex-cli", name: "Codex CLI", enabled: "codexEnabled", monitor: codexDetector, plan: "codexPlanId", legacyPlan: "codexPlan", limit: "codexCustomLimit", notifications: "codexNotifications" },
            { key: "github-copilot", name: "GitHub Copilot", enabled: "copilotEnabled", monitor: copilotDetector, plan: "copilotPlanId", legacyPlan: "copilotPlan", limit: "copilotCustomLimit", notifications: "copilotNotifications", billingMode: "copilotBillingMode", resetDay: "copilotResetDay", orgName: "copilotOrgName" },
            { key: "cursor", name: "Cursor", enabled: "cursorEnabled", monitor: cursorDetector, plan: "cursorPlanId", legacyPlan: "cursorPlan", limit: "cursorCustomLimit", notifications: "cursorNotifications" },
            { key: "windsurf", name: "Windsurf", enabled: "windsurfEnabled", monitor: windsurfDetector, plan: "windsurfPlanId", legacyPlan: "windsurfPlan", limit: "windsurfCustomLimit", notifications: "windsurfNotifications" },
            { key: "jetbrains-ai", name: "JetBrains AI", enabled: "jetbrainsAiEnabled", monitor: jetbrainsDetector, plan: "jetbrainsAiPlanId", legacyPlan: "jetbrainsAiPlan", limit: "jetbrainsAiCustomLimit", notifications: "jetbrainsAiNotifications" }
        ];
        var result = [];
        for (var i = 0; i < candidates.length; ++i) {
            var row = candidates[i];
            result.push({
                configKey: row.key,
                name: row.name,
                sourceKind: "ai_tool",
                monitoringLevel: row.monitoringLevel || "local_activity_estimate",
                enabledConfigKey: row.enabled,
                modelConfigKey: row.plan || "",
                legacyPlanConfigKey: row.legacyPlan || "",
                customLimitConfigKey: row.limit || "",
                notificationsConfigKey: row.notifications || "",
                billingModeConfigKey: row.billingMode || "",
                refreshIntervalConfigKey: row.refreshInterval || "",
                resetDayConfigKey: row.resetDay || "",
                orgNameConfigKey: row.orgName || "",
                monitor: row.monitor,
                customBaseUrlConfigKey: "",
                auth: { scheme: "none", credentialSlots: [] },
                installed: row.monitor.installed,
                safeRefresh: { method: "LOCAL", paths: [] }
            });
        }
        return result;
    }

    readonly property var settingsSources: descriptorCatalog.providers.concat(detectedLocalSources)

    ProviderSettingsController {
        id: settingsController
        descriptors: providersPage.settingsSources
        configuration: providersPage
        selectedSourceId: Plasmoid.configuration.settingsVerificationSourceId || ""
        onConfigurationEdited: providersPage.configurationChanged()
    }

    SecretsManager { id: secrets }
    BrowserSyncService {
        id: browserSync
        browserType: providersPage.cfg_browserSyncBrowser
        selectedFirefoxProfile: providersPage.cfg_browserSyncProfile
    }

    function reloadBrowserProfiles() {
        browserTestResults = ({});
        var entries = [i18n("Auto (Default Profile)")];
        var profiles = browserSync.browserProfiles();
        for (var i = 0; i < profiles.length; ++i) entries.push(profiles[i]);
        browserProfileModel = entries;
        var saved = providersPage.cfg_browserSyncProfile;
        if (saved && entries.indexOf(saved) < 0) {
            settingsController.setValue("browserSyncProfile", "");
        }
    }
    property var browserProfileModel: [i18n("Auto (Default Profile)")]

    SecretChangeSet {
        id: secretChanges
        store: secrets
    }

    function catalogModelIds(providerKey) {
        var rows = ProviderPricingCatalog.selectableModelsForProvider(providerKey);
        var ids = [];
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].id) ids.push(rows[i].id);
        }
        return ids;
    }

    function isInvalidUrl(url) {
        if (!url || url.length === 0) return false;
        var lower = url.toLowerCase();
        return !lower.startsWith("https://")
            && !lower.startsWith("http://localhost")
            && !lower.startsWith("http://127.0.0.1")
            && !lower.startsWith("http://[::1]");
    }

    function stageSecret(key, value) {
        if (value.length > 0) secretChanges.stageStore(key, value);
        else secretChanges.unstage(key);
        secretStatusMessage = "";
        secretStatusError = false;
        configurationChanged();
    }

    function removeSecret(key) {
        secretChanges.stageRemove(key);
        secretStatusMessage = "";
        secretStatusError = false;
        configurationChanged();
    }

    function hasStoredOrPendingSecret(key) {
        return secretChanges.pendingChanges[key] !== undefined
            || (secrets.walletOpen && secrets.hasKey(key));
    }

    function secretPlaceholder(key) {
        var pending = secretChanges.pendingChanges[key];
        if (pending !== undefined)
            return pending.action === "remove" ? i18n("Removal pending") : i18n("Replacement pending");
        return secrets.walletOpen && secrets.hasKey(key)
            ? i18n("Saved in KDE Wallet — leave blank to keep") : i18n("Required");
    }

    function optionalSecretPlaceholder(key) {
        if (secretChanges.pendingChanges[key] !== undefined)
            return secretPlaceholder(key);
        return secrets.walletOpen && secrets.hasKey(key)
            ? i18n("Saved in KDE Wallet — leave blank to keep")
            : i18n("Optional — stored securely in KDE Wallet");
    }

    function saveConfig() {
        var result = secretChanges.commit();
        secretStatusError = !result.ok;
        if (result.ok) {
            secretStatusMessage = result.appliedKeys.length > 0
                ? i18n("Credentials saved securely in KDE Wallet.") : "";
            if (result.appliedKeys.length > 0) secretCommitRevision++;
            settingsController.acceptChanges();
        } else if (result.message === "wallet-not-open") {
            secretStatusMessage = i18n("KDE Wallet is not open. Unlock it and retry Apply.");
        } else {
            secretStatusMessage = i18n("Some credentials could not be saved. Retry Apply.");
        }
        if (!result.ok) {
            providersPage.retainDraftOnDeparture = true;
            applyGuard.protectFailedApply();
            Qt.callLater(function() { providersPage.configurationChanged(); });
        }
    }

    function sourceQuotaStatus(sourceId) {
        if (sourceId !== "google-antigravity") return "";
        return antigravityDetector.syncStatus || i18n("No quota refresh has been reported.");
    }

    function sourceDetectedPlan(sourceId) {
        if (sourceId !== "google-antigravity") return "";
        return antigravityDetector.detectedPlanLabel || i18n("Detected automatically");
    }

    function sourceConnectionState(sourceId) {
        if (sourceId !== "google-antigravity") return "";
        return antigravityDetector.connectionState || i18n("Unknown");
    }

    function sourceLastSuccessfulRefresh(sourceId) {
        if (sourceId !== "google-antigravity") return "";
        var timestamp = antigravityDetector.lastSuccessfulRefresh;
        return timestamp && timestamp.getTime && timestamp.getTime() > 0
            ? timestamp.toLocaleString()
            : i18n("No successful refresh yet");
    }

    function refreshSourceQuota(sourceId) {
        if (sourceId === "google-antigravity") antigravityDetector.refreshQuota();
    }

    function normalizedSyncCode(code) {
        if (code === "not_found") return "cookies_not_found";
        if (code === "expired") return "session_missing_or_expired";
        return code;
    }

    function syncStatusColor(code) {
        var normalized = normalizedSyncCode(code);
        if (normalized === "connected") return Kirigami.Theme.positiveTextColor;
        if (normalized === "session_missing_or_expired") return Kirigami.Theme.neutralTextColor;
        return Kirigami.Theme.negativeTextColor;
    }

    function syncGuidance(code, serviceLabel) {
        var normalized = normalizedSyncCode(code);
        if (normalized === "connected") return i18n("%1 session looks valid in the selected browser.", serviceLabel);
        if (normalized === "profile_missing") return i18n("Choose a supported browser profile or open the browser once.");
        if (normalized === "cookie_db_missing") return i18n("Open the browser once, sign in to %1, then retry so the cookie database exists.", serviceLabel);
        if (normalized === "cookies_not_found") return i18n("Open %1 in the selected browser and sign in at least once.", serviceLabel);
        if (normalized === "session_missing_or_expired") return i18n("Log in to %1 again in the selected browser, then retry.", serviceLabel);
        if (normalized === "unsupported_browser") return i18n("The selected browser profile is not supported for sync.");
        return i18n("Check the browser session and retry.");
    }

    function testBrowserSession(service, serviceLabel) {
        if (hasUnsavedChanges || !cfg_browserSyncEnabled) return;
        var code = normalizedSyncCode(browserSync.testConnection(service));
        var result = {
            code: code,
            message: browserSync.connectionMessage(service, code),
            guidance: syncGuidance(code, serviceLabel)
        };
        var next = Object.assign({}, browserTestResults);
        next[service] = result;
        browserTestResults = next;
    }

    function requestVerification(sourceId) {
        if (hasUnsavedChanges || !sourceId) return;
        Plasmoid.configuration.settingsVerificationSourceId = sourceId;
        Plasmoid.configuration.settingsVerificationState = "verifying";
        Plasmoid.configuration.settingsVerificationMessage = i18n("Verification requested from the running widget.");
        Plasmoid.configuration.settingsVerificationTimestamp = "";
        Plasmoid.configuration.settingsVerificationRequestId =
            Number(Plasmoid.configuration.settingsVerificationRequestId || 0) + 1;
    }

    Connections {
        target: providersPage.Window.window
        enabled: target !== null
        function onClosing(event) {
            if (applyGuard.failedApplyInProgress) return;
            settingsController.discardChanges();
            secretChanges.discard();
            providersPage.secretStatusMessage = "";
            providersPage.secretStatusError = false;
        }
    }

    Component.onDestruction: secretChanges.discard()
    Component.onCompleted: reloadBrowserProfiles()

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            visible: providersPage.secretStatusMessage.length > 0
            text: providersPage.secretStatusMessage
            type: providersPage.secretStatusError
                ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 64
            Layout.alignment: Qt.AlignHCenter
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 64
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Configure API services and detected AI tools. Tool entries remain available when the application is not installed.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        RowLayout {
            objectName: "sourceSettingsColumns"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 64
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing

            ProviderSourceList {
                id: sourcePicker
                controller: settingsController
                Layout.preferredWidth: providersPage.compactSources ? parent.width : Kirigami.Units.gridUnit * 16
                Layout.minimumWidth: providersPage.compactSources ? 0 : Kirigami.Units.gridUnit * 12
                Layout.fillHeight: true
                visible: !providersPage.compactSources || !providersPage.showSourceDetails
                compact: providersPage.compactSources
                onSourceChosen: if (providersPage.compactSources) providersPage.showSourceDetails = true
            }

            Kirigami.Separator {
                Layout.fillHeight: true
                visible: !providersPage.compactSources
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !providersPage.compactSources || providersPage.showSourceDetails

                QQC2.Button {
                    visible: providersPage.compactSources
                    text: i18n("Back to sources")
                    icon.name: "go-previous"
                    onClicked: {
                        providersPage.showSourceDetails = false;
                        Qt.callLater(sourcePicker.focusSelectedSource);
                    }
                }

                QQC2.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        ProviderSourceDetails {
                            Layout.fillWidth: true
                            visible: settingsController.selectedSource.sourceKind !== "ai_tool"
                            descriptor: settingsController.selectedSource
                            controller: settingsController
                            configPage: providersPage
                        }
                        ConfigSubscriptions {
                            Layout.fillWidth: true
                            visible: settingsController.selectedSource.sourceKind === "ai_tool"
                            descriptor: settingsController.selectedSource
                            controller: settingsController
                            configPage: providersPage
                        }
                    }
                }
            }
        }

        BrowserSyncSettings {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 64
            Layout.alignment: Qt.AlignHCenter
            controller: settingsController
            configPage: providersPage
        }
    }

}
