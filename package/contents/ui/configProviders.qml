import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: providersPage

    signal configurationChanged()

    property bool cfg_advancedSettingsMode: plasmoid.configuration.advancedSettingsMode

    property bool cfg_openaiEnabled: plasmoid.configuration.openaiEnabled
    property string cfg_openaiModel: plasmoid.configuration.openaiModel
    property string cfg_openaiProjectId: plasmoid.configuration.openaiProjectId
    property string cfg_openaiCustomBaseUrl: plasmoid.configuration.openaiCustomBaseUrl
    property bool cfg_anthropicEnabled: plasmoid.configuration.anthropicEnabled
    property string cfg_anthropicModel: plasmoid.configuration.anthropicModel
    property string cfg_anthropicCustomBaseUrl: plasmoid.configuration.anthropicCustomBaseUrl
    property bool cfg_googleEnabled: plasmoid.configuration.googleEnabled
    property string cfg_googleModel: plasmoid.configuration.googleModel
    property string cfg_googleTier: plasmoid.configuration.googleTier
    property string cfg_googleCustomBaseUrl: plasmoid.configuration.googleCustomBaseUrl
    property bool cfg_mistralEnabled: plasmoid.configuration.mistralEnabled
    property string cfg_mistralModel: plasmoid.configuration.mistralModel
    property string cfg_mistralCustomBaseUrl: plasmoid.configuration.mistralCustomBaseUrl
    property bool cfg_deepseekEnabled: plasmoid.configuration.deepseekEnabled
    property string cfg_deepseekModel: plasmoid.configuration.deepseekModel
    property string cfg_deepseekCustomBaseUrl: plasmoid.configuration.deepseekCustomBaseUrl
    property bool cfg_groqEnabled: plasmoid.configuration.groqEnabled
    property string cfg_groqModel: plasmoid.configuration.groqModel
    property string cfg_groqCustomBaseUrl: plasmoid.configuration.groqCustomBaseUrl
    property bool cfg_xaiEnabled: plasmoid.configuration.xaiEnabled
    property string cfg_xaiModel: plasmoid.configuration.xaiModel
    property string cfg_xaiCustomBaseUrl: plasmoid.configuration.xaiCustomBaseUrl
    property bool cfg_ollamaEnabled: plasmoid.configuration.ollamaEnabled
    property string cfg_ollamaModel: plasmoid.configuration.ollamaModel
    property string cfg_ollamaCustomBaseUrl: plasmoid.configuration.ollamaCustomBaseUrl
    property bool cfg_openrouterEnabled: plasmoid.configuration.openrouterEnabled
    property string cfg_openrouterModel: plasmoid.configuration.openrouterModel
    property string cfg_openrouterCustomBaseUrl: plasmoid.configuration.openrouterCustomBaseUrl
    property bool cfg_togetherEnabled: plasmoid.configuration.togetherEnabled
    property string cfg_togetherModel: plasmoid.configuration.togetherModel
    property string cfg_togetherCustomBaseUrl: plasmoid.configuration.togetherCustomBaseUrl
    property bool cfg_cohereEnabled: plasmoid.configuration.cohereEnabled
    property string cfg_cohereModel: plasmoid.configuration.cohereModel
    property string cfg_cohereCustomBaseUrl: plasmoid.configuration.cohereCustomBaseUrl
    property bool cfg_googleveoEnabled: plasmoid.configuration.googleveoEnabled
    property string cfg_googleveoModel: plasmoid.configuration.googleveoModel
    property string cfg_googleveoTier: plasmoid.configuration.googleveoTier
    property string cfg_googleveoCustomBaseUrl: plasmoid.configuration.googleveoCustomBaseUrl
    property bool cfg_azureEnabled: plasmoid.configuration.azureEnabled
    property string cfg_azureModel: plasmoid.configuration.azureModel
    property string cfg_azureDeploymentId: plasmoid.configuration.azureDeploymentId
    property string cfg_azureCustomBaseUrl: plasmoid.configuration.azureCustomBaseUrl
    property bool cfg_bedrockEnabled: plasmoid.configuration.bedrockEnabled
    property string cfg_bedrockRegion: plasmoid.configuration.bedrockRegion
    property string cfg_bedrockModel: plasmoid.configuration.bedrockModel
    property string cfg_bedrockCustomBaseUrl: plasmoid.configuration.bedrockCustomBaseUrl
    property bool cfg_litellmEnabled: plasmoid.configuration.litellmEnabled
    property string cfg_litellmModel: plasmoid.configuration.litellmModel
    property string cfg_litellmCustomBaseUrl: plasmoid.configuration.litellmCustomBaseUrl
    property bool cfg_cerebrasEnabled: plasmoid.configuration.cerebrasEnabled
    property string cfg_cerebrasModel: plasmoid.configuration.cerebrasModel
    property string cfg_cerebrasCustomBaseUrl: plasmoid.configuration.cerebrasCustomBaseUrl
    property bool cfg_fireworksEnabled: plasmoid.configuration.fireworksEnabled
    property string cfg_fireworksModel: plasmoid.configuration.fireworksModel
    property string cfg_fireworksCustomBaseUrl: plasmoid.configuration.fireworksCustomBaseUrl
    property bool cfg_perplexityEnabled: plasmoid.configuration.perplexityEnabled
    property string cfg_perplexityModel: plasmoid.configuration.perplexityModel
    property string cfg_perplexityCustomBaseUrl: plasmoid.configuration.perplexityCustomBaseUrl
    property bool cfg_claudeCodeEnabled: plasmoid.configuration.claudeCodeEnabled
    property bool cfg_codexEnabled: plasmoid.configuration.codexEnabled
    property bool cfg_copilotEnabled: plasmoid.configuration.copilotEnabled
    property bool cfg_cursorEnabled: plasmoid.configuration.cursorEnabled
    property bool cfg_windsurfEnabled: plasmoid.configuration.windsurfEnabled
    property bool cfg_jetbrainsAiEnabled: plasmoid.configuration.jetbrainsAiEnabled

    readonly property bool advancedMode: cfg_advancedSettingsMode
    readonly property bool walletOpen: secrets.walletOpen
    readonly property bool hasUnsavedChanges: settingsController.dirty || secretChanges.dirty
    readonly property bool unsavedChanges: secretChanges.dirty
    readonly property string verificationSourceId: plasmoid.configuration.settingsVerificationSourceId || ""
    readonly property string verificationState: plasmoid.configuration.settingsVerificationState || ""
    readonly property string verificationMessage: plasmoid.configuration.settingsVerificationMessage || ""
    readonly property string verificationTimestamp: plasmoid.configuration.settingsVerificationTimestamp || ""

    property string secretStatusMessage: ""
    property bool secretStatusError: false

    ProviderCatalog { id: descriptorCatalog }

    ClaudeCodeMonitor { id: claudeDetector; Component.onCompleted: checkToolInstalled() }
    CodexCliMonitor { id: codexDetector; Component.onCompleted: checkToolInstalled() }
    CopilotMonitor { id: copilotDetector; Component.onCompleted: checkToolInstalled() }
    CursorMonitor { id: cursorDetector; Component.onCompleted: checkToolInstalled() }
    WindsurfMonitor { id: windsurfDetector; Component.onCompleted: checkToolInstalled() }
    JetBrainsAiMonitor { id: jetbrainsDetector; Component.onCompleted: checkToolInstalled() }

    readonly property var detectedLocalSources: {
        var candidates = [
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
                monitoringLevel: "local_activity_estimate",
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
        selectedSourceId: plasmoid.configuration.settingsVerificationSourceId || ""
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
        plasmoid.configuration.settingsVerificationSourceId = sourceId;
        plasmoid.configuration.settingsVerificationState = "verifying";
        plasmoid.configuration.settingsVerificationMessage = i18n("Verification requested from the running widget.");
        plasmoid.configuration.settingsVerificationTimestamp = "";
        plasmoid.configuration.settingsVerificationRequestId =
            Number(plasmoid.configuration.settingsVerificationRequestId || 0) + 1;
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
