pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0
import QtQuick.Dialogs as Dialogs
import "ConfigPortability.js" as ConfigPortability

KCM.SimpleKCM {
    id: diagnosticsPage

    readonly property string versionCheckCommand: "plasmashell --version; rpm -q plasma-ai-usage-monitor"
    readonly property string troubleshootingUrl: "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/troubleshooting.md"
    readonly property string providerGuideUrl: "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/providers.md"
    readonly property string providerCatalogUrl: "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/package/contents/catalog/providers-v4.json"
    readonly property string subscriptionGuideUrl: "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/subscriptions.md"
    // Plasma's KPluginMetaData value type is absent from its installed qmltypes.
    // qmllint disable unresolved-type
    readonly property string frontendVersion:
        (Plasmoid["metaData"] && Plasmoid["metaData"]["version"])
        ? Plasmoid["metaData"]["version"] : "unknown"
    // qmllint enable unresolved-type
    readonly property var systemInfo: AppInfo.systemDiagnostics(frontendVersion)
    readonly property var databaseInfo: AppInfo.databaseDiagnostics()
    readonly property var sourceSnapshot: parseSourceSnapshot()
    property string recoverySelectionMessage: ""

    SecretsManager { id: secrets }
    BrowserSyncService { id: syncDetector }
    ProviderCatalog { id: providerCatalog }
    ClipboardHelper { id: clipboard }

    // Helpers to detect CLI tools
    ClaudeCodeMonitor { id: claudeDetector; Component.onCompleted: checkToolInstalled() }
    CodexCliMonitor { id: codexDetector; Component.onCompleted: checkToolInstalled() }
    CopilotMonitor { id: copilotDetector; Component.onCompleted: checkToolInstalled() }

    readonly property var portableConfigKeys: [
        "refreshInterval", "compactDisplayMode", "setupWizardCompleted", "setupWizardDismissed",
        "setupWizardInProgress", "setupWizardStep", "setupWizardGoal", "setupWizardSourceId", "advancedSettingsMode",
        "settingsVerificationRequestId", "settingsVerificationCompletedRequestId", "settingsVerificationSourceId",
        "settingsVerificationState", "settingsVerificationMessage", "settingsVerificationTimestamp",
        "diagnosticsSourceSnapshot",
        "openaiRefreshInterval", "anthropicRefreshInterval", "googleRefreshInterval",
        "mistralRefreshInterval", "deepseekRefreshInterval", "groqRefreshInterval", "xaiRefreshInterval", "ollamaRefreshInterval",
        "openrouterRefreshInterval", "togetherRefreshInterval", "cohereRefreshInterval", "googleveoRefreshInterval", "azureRefreshInterval",
        "bedrockRefreshInterval", "litellmRefreshInterval", "cerebrasRefreshInterval", "fireworksRefreshInterval", "perplexityRefreshInterval", "openaiEnabled", "openaiModel", "openaiProjectId", "openaiCustomBaseUrl",
        "azureEnabled", "azureModel", "azureDeploymentId", "azureCustomBaseUrl", "bedrockEnabled",
        "bedrockRegion", "bedrockModel", "bedrockCustomBaseUrl", "anthropicEnabled", "anthropicModel",
        "anthropicCustomBaseUrl", "googleEnabled", "googleModel", "googleTier", "googleCustomBaseUrl",
        "mistralEnabled", "mistralModel", "mistralCustomBaseUrl", "deepseekEnabled", "deepseekModel",
        "deepseekCustomBaseUrl", "groqEnabled", "groqModel", "groqCustomBaseUrl", "xaiEnabled",
        "xaiModel", "xaiCustomBaseUrl", "ollamaEnabled", "ollamaModel", "ollamaCustomBaseUrl",
        "openrouterEnabled", "openrouterModel", "openrouterCustomBaseUrl", "togetherEnabled", "togetherModel",
        "togetherCustomBaseUrl", "cohereEnabled", "cohereModel", "cohereCustomBaseUrl", "googleveoEnabled",
        "googleveoModel", "googleveoTier", "googleveoCustomBaseUrl", "litellmEnabled", "litellmModel", "litellmCustomBaseUrl", "cerebrasEnabled", "cerebrasModel", "cerebrasCustomBaseUrl", "fireworksEnabled", "fireworksModel", "fireworksCustomBaseUrl", "perplexityEnabled", "perplexityModel", "perplexityCustomBaseUrl", "alertsEnabled", "warningThreshold",
        "criticalThreshold", "notifyOnError", "notifyOnBudgetWarning", "notifyOnDisconnect", "notifyOnReconnect",
        "notificationCooldownMinutes", "dndStartHour", "dndEndHour", "openaiNotificationsEnabled", "anthropicNotificationsEnabled",
        "googleNotificationsEnabled", "mistralNotificationsEnabled", "deepseekNotificationsEnabled", "groqNotificationsEnabled", "xaiNotificationsEnabled",
        "ollamaNotificationsEnabled", "openrouterNotificationsEnabled", "togetherNotificationsEnabled", "cohereNotificationsEnabled", "googleveoNotificationsEnabled",
        "azureNotificationsEnabled", "bedrockNotificationsEnabled", "litellmNotificationsEnabled", "cerebrasNotificationsEnabled", "fireworksNotificationsEnabled", "perplexityNotificationsEnabled", "notifyOnUpdate", "updateCheckInterval", "slackWebhookEnabled",
        "discordWebhookEnabled", "webhookCooldownMinutes", "openaiDailyBudget", "openaiMonthlyBudget", "anthropicDailyBudget",
        "anthropicMonthlyBudget", "googleDailyBudget", "googleMonthlyBudget", "mistralDailyBudget", "mistralMonthlyBudget",
        "deepseekDailyBudget", "deepseekMonthlyBudget", "groqDailyBudget", "groqMonthlyBudget", "xaiDailyBudget",
        "xaiMonthlyBudget", "ollamaDailyBudget", "ollamaMonthlyBudget", "openrouterDailyBudget", "openrouterMonthlyBudget",
        "togetherDailyBudget", "togetherMonthlyBudget", "cohereDailyBudget", "cohereMonthlyBudget", "googleveoDailyBudget",
        "googleveoMonthlyBudget", "azureDailyBudget", "azureMonthlyBudget", "bedrockDailyBudget", "bedrockMonthlyBudget", "litellmDailyBudget", "litellmMonthlyBudget", "cerebrasDailyBudget", "cerebrasMonthlyBudget", "fireworksDailyBudget", "fireworksMonthlyBudget", "perplexityDailyBudget", "perplexityMonthlyBudget",
        "budgetWarningPercent", "forecastUiEnabled", "forecastNotificationsEnabled", "forecastLeadTimeHours",
        "historyEnabled", "historyRetentionDays", "analystIntensityMode", "analystNormalization",
        "prometheusEnabled", "prometheusPort", "autoExportEnabled", "autoExportDirectory", "autoExportIntervalMinutes",
        "autoExportFormat", "antigravityEnabled", "antigravityNotifications", "antigravityRefreshInterval",
        "browserSyncEnabled", "browserSyncBrowser", "browserSyncProfile", "browserSyncInterval",
        "claudeCodeEnabled", "claudeCodePlan", "claudeCodePlanId", "claudeCodeCustomLimit", "claudeCodeNotifications",
        "codexEnabled", "codexPlan", "codexPlanId", "codexCustomLimit", "codexNotifications",
        "copilotEnabled", "copilotPlan", "copilotPlanId", "copilotCustomLimit", "copilotBillingMode",
        "copilotResetDay", "copilotNotifications", "copilotOrgName", "cursorEnabled", "cursorPlan",
        "cursorPlanId", "cursorCustomLimit", "cursorNotifications", "windsurfEnabled", "windsurfPlan",
        "windsurfPlanId", "windsurfCustomLimit", "windsurfNotifications", "jetbrainsAiEnabled", "jetbrainsAiPlan",
        "jetbrainsAiPlanId", "jetbrainsAiCustomLimit", "jetbrainsAiNotifications"
    ]

    readonly property var ignoredLegacyConfigKeys: [
        "dashboardMode", "showOnlyProblems"
    ]

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export Configuration")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["JSON Files (*.json)"]
        currentFile: "ai-usage-monitor-config.json"
        onAccepted: {
            AppInfo.exportConfig(JSON.stringify(diagnosticsPage.exportConfigData(), null, 2), selectedFile.toString());
        }
    }

    Dialogs.FileDialog {
        id: importDialog
        title: i18n("Import Configuration")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["JSON Files (*.json)"]
        onAccepted: {
            var jsonStr = AppInfo.importConfig(selectedFile.toString());
            if (jsonStr.length > 0) {
                try {
                    var configData = JSON.parse(jsonStr);
                    diagnosticsPage.importConfigData(configData);
                } catch (e) {
                    console.error("Failed to parse imported config:", e);
                }
            }
        }
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Trust Center")
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Actual API Usage:")
            text: i18n("Provider-reported usage or billing endpoints when available.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Estimated Cost:")
            text: i18n("Calculated locally from token counts and the shipped Provider Catalog.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Rate Limit Only:")
            text: i18n("Connectivity and quota headers without exact provider billing data.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Background Traffic:")
            text: i18n("Scheduled provider monitoring uses read-only GET endpoints only. Inference tests are manual and may consume quota or money.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Scheduled calls:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: providerCatalog.providers
                delegate: QQC2.Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name + ": " + diagnosticsPage.scheduledCalls(modelData.safeRefresh)
                          + " · " + modelData.minimumRefreshSeconds + "s minimum · read-only"
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Local Tool Data:")
            text: i18n("Filesystem-derived activity for subscription tools; self-tracked, not vendor billing truth.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Browser Sync Labs:")
            text: i18n("Experimental browser-session probes. Cookies and tokens are not logged; local estimation remains the fallback.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        CatalogTrustPanel {
            Kirigami.FormData.label: i18n("Catalogs:")
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Catalog Actions:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: i18n("Open provider guide")
                icon.name: "help-contents"
                onClicked: Qt.openUrlExternally(diagnosticsPage.providerGuideUrl)
            }

            QQC2.Button {
                text: i18n("Review provider catalog")
                icon.name: "document-open"
                onClicked: Qt.openUrlExternally(diagnosticsPage.providerCatalogUrl)
            }
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Provider Health:")
            text: i18n("%1 providers enabled", diagnosticsPage.enabledProviderCount())
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Wallet & Secrets")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("KWallet Status:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: secrets.walletOpen ? "dialog-ok" : "dialog-error"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: secrets.walletOpen ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: secrets.walletOpen ? i18n("Wallet is open and accessible") : i18n("Wallet is closed or inaccessible. API keys cannot be saved or loaded.")
                color: secrets.walletOpen ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
                wrapMode: Text.WordWrap
            }

            QQC2.Button {
                text: secrets.walletOpen ? i18n("Reload") : i18n("Open Wallet")
                icon.name: "wallet-open"
                onClicked: secrets.retryOpenWallet()
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Copilot Token:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: secrets.walletOpen && secrets.hasKey("copilot_github") ? "dialog-ok" : "dialog-warning"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: secrets.walletOpen && secrets.hasKey("copilot_github") ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: secrets.walletOpen && secrets.hasKey("copilot_github")
                    ? i18n("GitHub token is available for organization metrics")
                    : i18n("No GitHub token loaded for Copilot organization metrics")
                wrapMode: Text.WordWrap
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Browser Sync Readiness")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Browser Profiles:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: syncDetector.hasCurrentBrowserProfile ? "dialog-ok" : "dialog-warning"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: syncDetector.hasCurrentBrowserProfile ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: syncDetector.hasCurrentBrowserProfile ? i18n("Found browser profiles for sync") : i18n("No supported browser profile found")
                color: syncDetector.hasCurrentBrowserProfile ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
                wrapMode: Text.WordWrap
            }

            QQC2.Button {
                text: i18n("Choose profile")
                icon.name: "folder-open"
                onClicked: Plasmoid.internalAction("configure").trigger()
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Cookie DB:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: syncDetector.readinessReport("").cookieDatabaseReadable ? "dialog-ok" : "dialog-warning"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: syncDetector.readinessReport("").cookieDatabaseReadable ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: syncDetector.readinessReport("").cookieDatabaseReadable ? i18n("Readable") : syncDetector.readinessReport("").nextStep
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("KWallet/libsecret:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: syncDetector.hasSafeStorageAccess ? "dialog-ok" : "dialog-warning"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: syncDetector.hasSafeStorageAccess ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: syncDetector.hasSafeStorageAccess ? i18n("Ready for selected browser") : i18n("Safe storage is locked or unavailable; use local estimation.")
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Custom URLs:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: diagnosticsPage.insecureCustomUrlCount() === 0 ? "dialog-ok" : "dialog-warning"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: diagnosticsPage.insecureCustomUrlCount() === 0 ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: diagnosticsPage.insecureCustomUrlCount() === 0
                    ? i18n("No insecure remote custom API URLs configured")
                    : i18n("%1 insecure remote custom API URLs need review", diagnosticsPage.insecureCustomUrlCount())
                wrapMode: Text.WordWrap
            }

            QQC2.Button {
                text: i18n("Review URLs")
                icon.name: "configure"
                enabled: diagnosticsPage.insecureCustomUrlCount() > 0
                onClicked: Plasmoid.internalAction("configure").trigger()
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Local Dependencies")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Claude Code:")
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label { text: claudeDetector.installed ? "✓ " + i18n("Installed") : "✗ " + i18n("Not found") }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Codex CLI:")
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label { text: codexDetector.installed ? "✓ " + i18n("Installed") : "✗ " + i18n("Not found") }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("GitHub Copilot:")
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label { text: copilotDetector.installed ? "✓ " + i18n("Installed") : "✗ " + i18n("Not found") }
        }
        
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Config Portability")
        }

        QQC2.Label {
            text: i18n("Exports schema v2 non-secret settings only. API keys, tokens, cookies, PATs, and webhook URLs remain in KWallet and are never written to the file.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.6
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Configuration:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: i18n("Export...")
                icon.name: "document-export"
                onClicked: {
                    exportDialog.open();
                }
            }

            QQC2.Button {
                text: i18n("Import...")
                icon.name: "document-import"
                onClicked: {
                    importDialog.open();
                }
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Diagnostics")
        }
        
        QQC2.Label {
            Kirigami.FormData.label: i18n("Version:")
            text: i18n("Frontend %1 · native plugin %2", diagnosticsPage.frontendVersion, AppInfo.version)
            color: diagnosticsPage.systemInfo.nativeStatus === "ready"
                   ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Loaded plugin:")
            text: diagnosticsPage.systemInfo.nativePluginPath
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Install layers:")
            text: i18n("Frontend: %1 · plugin: %2", diagnosticsPage.systemInfo.frontendLayer,
                       diagnosticsPage.systemInfo.pluginLayer)
                  + (diagnosticsPage.systemInfo.shadowing ? i18n(" · user-local package shadows system package") : "")
            color: diagnosticsPage.systemInfo.shadowing
                   ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("System:")
            text: diagnosticsPage.systemInfo.plasmaVersion + " · " + diagnosticsPage.systemInfo.distribution
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("History database:")
            text: diagnosticsPage.databaseStatusText()
            color: ["ok", "not_created"].indexOf(diagnosticsPage.databaseInfo.status) >= 0
                   ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Kirigami.InlineMessage {
            Kirigami.FormData.label: i18n("Recovery:")
            Layout.fillWidth: true
            visible: diagnosticsPage.systemInfo.nativeStatus !== "ready"
                     || diagnosticsPage.systemInfo.shadowing
            text: diagnosticsPage.systemInfo.nextStep
            type: Kirigami.MessageType.Warning
            actions: [
                Kirigami.Action {
                    text: i18n("Copy repair command")
                    icon.name: "edit-copy"
                    visible: diagnosticsPage.systemInfo.repairCommand.length > 0
                    onTriggered: clipboard.setText(diagnosticsPage.systemInfo.repairCommand)
                }
            ]
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Source readiness:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: diagnosticsPage.actionableSources()

                delegate: RowLayout {
                    id: sourceRow
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: diagnosticsPage.sourceSummary(sourceRow.modelData)
                        wrapMode: Text.WordWrap
                    }

                    QQC2.Button {
                        text: sourceRow.modelData.sourceKindKey === "local_tool"
                              ? i18n("Open tool guide") : i18n("Select in Providers")
                        icon.name: "configure"
                        visible: sourceRow.modelData.nextActionKey !== "none"
                        onClicked: {
                            if (sourceRow.modelData.sourceKindKey === "local_tool") {
                                Qt.openUrlExternally(diagnosticsPage.subscriptionGuideUrl);
                                return;
                            }
                            Plasmoid.configuration.settingsVerificationSourceId = sourceRow.modelData.stableId;
                            diagnosticsPage.recoverySelectionMessage = i18n("%1 is selected. Open Providers in the sidebar to continue.", diagnosticsPage.sourceDisplayName(sourceRow.modelData.stableId));
                        }
                    }
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: diagnosticsPage.actionableSources().length === 0
                text: i18n("No enabled source currently needs recovery.")
                color: Kirigami.Theme.positiveTextColor
                wrapMode: Text.WordWrap
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: diagnosticsPage.recoverySelectionMessage.length > 0
                text: diagnosticsPage.recoverySelectionMessage
                color: Kirigami.Theme.linkColor
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Actions:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: i18n("Open troubleshooting")
                icon.name: "help-contents"
                onClicked: Qt.openUrlExternally(diagnosticsPage.troubleshootingUrl)
            }

            QQC2.Button {
                text: i18n("Copy version check")
                icon.name: "edit-copy"
                onClicked: {
                    clipboard.setText(diagnosticsPage.versionCheckCommand);
                }
            }

            QQC2.Button {
                text: i18n("Copy support report")
                icon.name: "edit-copy"
                onClicked: clipboard.setText(diagnosticsPage.buildSupportReport())
            }

            QQC2.Button {
                text: i18n("Copy capability report")
                icon.name: "documentinfo"
                onClicked: clipboard.setText(diagnosticsPage.buildCapabilityReport())
            }
        }
    }

    function exportConfigData() {
        var settings = {};
        for (var i = 0; i < portableConfigKeys.length; i++) {
            var key = portableConfigKeys[i];
            settings[key] = Plasmoid.configuration[key];
        }
        return {
            schemaVersion: 2,
            app: "com.github.loofi.aiusagemonitor",
            exportedByVersion: AppInfo.version,
            includesSecrets: false,
            settings: settings
        };
    }

    function importConfigData(configData) {
        if (configData.schemaVersion === 2 && configData.settings) {
            var settings = ConfigPortability.schemaV2Settings(
                configData, portableConfigKeys);
            var keys = Object.keys(settings);
            for (var i = 0; i < keys.length; ++i) {
                var key = keys[i];
                Plasmoid.configuration[key] = settings[key];
            }
            return;
        }

        if (configData.general) {
            if (configData.general.refreshInterval !== undefined) Plasmoid.configuration.refreshInterval = configData.general.refreshInterval;
            if (configData.general.compactDisplayMode !== undefined) Plasmoid.configuration.compactDisplayMode = configData.general.compactDisplayMode;
        }
        var legacyKeys = [
            "openaiEnabled", "openaiModel", "anthropicEnabled", "anthropicModel",
            "googleEnabled", "googleModel", "ollamaEnabled"
        ];
        for (var j = 0; j < legacyKeys.length; j++) {
            var legacyKey = legacyKeys[j];
            if (configData[legacyKey] !== undefined) {
                Plasmoid.configuration[legacyKey] = configData[legacyKey];
            }
        }
    }

    function enabledProviderCount() {
        var count = 0;
        var rows = providerCatalog.providers || [];
        for (var i = 0; i < rows.length; i++) {
            if (Plasmoid.configuration[rows[i].enabledConfigKey]) {
                count++;
            }
        }
        return count;
    }

    function parseSourceSnapshot() {
        var raw = Plasmoid.configuration.diagnosticsSourceSnapshot || "[]";
        try {
            var parsed = JSON.parse(raw);
            return Array.isArray(parsed) ? parsed : [];
        } catch (error) {
            return [];
        }
    }

    function actionableSources() {
        var result = [];
        for (var i = 0; i < sourceSnapshot.length; i++) {
            var source = sourceSnapshot[i];
            if (!source.enabled) continue;
            if (["reporting_actual", "reporting_estimate", "connected_connectivity_only"].indexOf(source.readinessStateKey) >= 0)
                continue;
            result.push(source);
        }
        return result;
    }

    function sourceDisplayName(stableId) {
        var rows = providerCatalog.providers || [];
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].configKey === stableId) return rows[i].name;
        }
        var localNames = {
            "google-antigravity": "Google Antigravity",
            "claude-code": "Claude Code", "codex-cli": "Codex CLI",
            "github-copilot": "GitHub Copilot", "cursor": "Cursor",
            "windsurf": "Windsurf", "jetbrains-ai": "JetBrains AI"
        };
        return localNames[stableId] || stableId;
    }

    function sourceSummary(source) {
        var states = {
            "disabled": i18n("Disabled"),
            "unavailable_locally": i18n("Not installed locally"),
            "needs_configuration": i18n("Needs configuration"),
            "ready_to_verify": i18n("Ready to verify"),
            "verifying": i18n("Verifying"),
            "connected_connectivity_only": i18n("Connectivity confirmed"),
            "reporting_estimate": i18n("Reporting an estimate"),
            "reporting_actual": i18n("Reporting provider data"),
            "degraded": i18n("Needs attention"),
            "failed": i18n("Verification failed")
        };
        var errors = {
            "backend_unavailable": i18n("native backend unavailable"),
            "configuration": i18n("configuration incomplete"),
            "authentication": i18n("authentication failed"),
            "not_logged_in": i18n("not signed in"),
            "not_signed_in": i18n("not signed in"),
            "daemon_not_running": i18n("local daemon is not running"),
            "unsupported_version": i18n("installed version is not supported"),
            "tls_error": i18n("local TLS verification failed"),
            "timeout": i18n("local daemon timed out"),
            "permission": i18n("permission denied"),
            "permission_denied": i18n("permission denied"),
            "unsupported_metric": i18n("metric unsupported"),
            "not_supported": i18n("metric unsupported"),
            "schema": i18n("provider response changed"),
            "stale": i18n("data is stale"),
            "network": i18n("network error"),
            "network_error": i18n("network error"),
            "timeout": i18n("request timed out"),
            "rate_limit": i18n("rate limited"),
            "server": i18n("provider server error")
        };
        var error = source.errorCode
            ? " · " + (errors[source.errorCode] || i18n("error: %1", source.errorCode)) : "";
        return sourceDisplayName(source.stableId) + ": "
            + (states[source.readinessStateKey] || i18n("Unknown")) + error;
    }

    function formatBytes(bytes) {
        var value = Number(bytes || 0);
        if (value < 1024) return i18n("%1 B", value);
        if (value < 1024 * 1024) return i18n("%1 KiB", (value / 1024).toFixed(1));
        return i18n("%1 MiB", (value / (1024 * 1024)).toFixed(1));
    }

    function databaseStatusText() {
        var labels = {
            "ok": i18n("Healthy"),
            "not_created": i18n("Not created yet"),
            "unreadable": i18n("Unreadable"),
            "open_failed": i18n("Could not be opened read-only"),
            "integrity_failed": i18n("Integrity check failed")
        };
        return (labels[databaseInfo.status] || i18n("Unknown")) + " · " + formatBytes(databaseInfo.sizeBytes);
    }

    function insecureCustomUrlCount() {
        var keys = [
            "openaiCustomBaseUrl", "anthropicCustomBaseUrl", "googleCustomBaseUrl",
            "mistralCustomBaseUrl", "deepseekCustomBaseUrl", "groqCustomBaseUrl",
            "xaiCustomBaseUrl", "ollamaCustomBaseUrl", "openrouterCustomBaseUrl",
            "togetherCustomBaseUrl", "cohereCustomBaseUrl", "googleveoCustomBaseUrl",
            "azureCustomBaseUrl", "bedrockCustomBaseUrl", "litellmCustomBaseUrl",
            "cerebrasCustomBaseUrl", "fireworksCustomBaseUrl", "perplexityCustomBaseUrl"
        ];
        var count = 0;
        for (var i = 0; i < keys.length; i++) {
            var url = (Plasmoid.configuration[keys[i]] || "").toString().trim();
            if (url.indexOf("http://") === 0
                && url.indexOf("http://localhost") !== 0
                && url.indexOf("http://127.0.0.1") !== 0
                && url.indexOf("http://[::1]") !== 0) {
                count++;
            }
        }
        return count;
    }

    function scheduledCalls(safeRefresh) {
        var paths = safeRefresh.paths || [safeRefresh.path || ""];
        var calls = [];
        for (var i = 0; i < paths.length; i++) {
            if (paths[i]) calls.push((safeRefresh.method || "GET") + " " + paths[i]);
        }
        return calls.join("; ");
    }

    function buildCapabilityReport() {
        var lines = ["Plasma AI Usage Monitor Provider Capability Report", "Version: " + AppInfo.version];
        var rows = providerCatalog.providers || [];
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            lines.push(row.configKey
                       + " enabled=" + (!!Plasmoid.configuration[row.enabledConfigKey] ? "yes" : "no")
                       + " level=" + row.monitoringLevel
                       + " scheduled=\"" + scheduledCalls(row.safeRefresh) + "\""
                       + " min_interval=" + row.minimumRefreshSeconds
                       + " request_budget=" + row.requestBudget);
        }
        lines.push("Endpoint hosts, query strings, account IDs, project IDs, credentials, and KWallet values are redacted.");
        return lines.join("\n");
    }

    function buildSupportReport() {
        return AppInfo.buildSupportReport(diagnosticsPage.frontendVersion, {
            walletOpen: secrets.walletOpen,
            providerCatalogVersion: ProviderPricingCatalog.catalogVersion,
            subscriptionCatalogVersion: SubscriptionPlanCatalog.catalogVersion,
            sources: diagnosticsPage.sourceSnapshot
        });
    }
}
