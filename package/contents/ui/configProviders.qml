import QtQuick
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: providersPage

    signal configurationChanged()

    property bool cfg_advancedSettingsMode: Plasmoid.configuration.advancedSettingsMode

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

    readonly property bool advancedMode: cfg_advancedSettingsMode
    readonly property bool walletOpen: secrets.walletOpen
    readonly property bool hasUnsavedChanges: settingsController.dirty || secretChanges.dirty
    readonly property bool unsavedChanges: secretChanges.dirty
    readonly property string verificationSourceId: Plasmoid.configuration.settingsVerificationSourceId || ""
    readonly property string verificationState: Plasmoid.configuration.settingsVerificationState || ""
    readonly property string verificationMessage: Plasmoid.configuration.settingsVerificationMessage || ""
    readonly property string verificationTimestamp: Plasmoid.configuration.settingsVerificationTimestamp || ""

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
              monitoringLevel: "actual_quota" },
            { key: "claude-code", name: "Claude Code", enabled: "claudeCodeEnabled", monitor: claudeDetector },
            { key: "codex-cli", name: "Codex CLI", enabled: "codexEnabled", monitor: codexDetector },
            { key: "github-copilot", name: "GitHub Copilot", enabled: "copilotEnabled", monitor: copilotDetector },
            { key: "cursor", name: "Cursor", enabled: "cursorEnabled", monitor: cursorDetector },
            { key: "windsurf", name: "Windsurf", enabled: "windsurfEnabled", monitor: windsurfDetector },
            { key: "jetbrains-ai", name: "JetBrains AI", enabled: "jetbrainsAiEnabled", monitor: jetbrainsDetector }
        ];
        var result = [];
        for (var i = 0; i < candidates.length; ++i) {
            var row = candidates[i];
            if (!row.monitor.installed) continue;
            result.push({
                configKey: row.key,
                name: row.name,
                sourceKind: "local_tool",
                monitoringLevel: row.monitoringLevel || "local_activity_estimate",
                enabledConfigKey: row.enabled,
                modelConfigKey: "",
                customBaseUrlConfigKey: "",
                auth: { scheme: "none", credentialSlots: [] },
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

    function saveConfig() {
        var result = secretChanges.commit();
        secretStatusError = !result.ok;
        if (result.ok) {
            secretStatusMessage = result.appliedKeys.length > 0
                ? i18n("Credentials saved securely in KDE Wallet.") : "";
            settingsController.acceptChanges();
        } else if (result.message === "wallet-not-open") {
            secretStatusMessage = i18n("KDE Wallet is not open. Unlock it and retry Apply.");
        } else {
            secretStatusMessage = i18n("Some credentials could not be saved. Retry Apply.");
        }
        if (!result.ok) Qt.callLater(function() { providersPage.configurationChanged(); });
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

    Component.onDestruction: secretChanges.discard()

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            visible: providersPage.secretStatusMessage.length > 0
            text: providersPage.secretStatusMessage
            type: providersPage.secretStatusError
                ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                text: i18n("Provider sources")
            }

            QQC2.Switch {
                text: i18n("Advanced")
                checked: providersPage.advancedMode
                Accessible.name: i18n("Show advanced provider settings")
                onToggled: settingsController.setValue("advancedSettingsMode", checked)
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Choose one source to configure. Usage and spend sources are listed before connectivity-only checks.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.largeSpacing

            ProviderSourceList {
                controller: settingsController
                Layout.preferredWidth: Kirigami.Units.gridUnit * 15
                Layout.minimumWidth: Kirigami.Units.gridUnit * 12
                Layout.fillHeight: true
            }

            Kirigami.Separator {
                Layout.fillHeight: true
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                ProviderSourceDetails {
                    width: parent.width
                    descriptor: settingsController.selectedSource
                    controller: settingsController
                    configPage: providersPage
                }
            }
        }
    }
}
