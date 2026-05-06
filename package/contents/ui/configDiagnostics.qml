import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0
import QtQuick.Dialogs as Dialogs

KCM.SimpleKCM {
    id: diagnosticsPage

    SecretsManager { id: secrets }
    BrowserCookieExtractor { id: syncDetector }
    ProviderCatalog { id: providerCatalog }

    // Helpers to detect CLI tools
    ClaudeCodeMonitor { id: claudeDetector; Component.onCompleted: checkToolInstalled() }
    CodexCliMonitor { id: codexDetector; Component.onCompleted: checkToolInstalled() }
    CopilotMonitor { id: copilotDetector; Component.onCompleted: checkToolInstalled() }

    
    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export Configuration")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["JSON Files (*.json)"]
        currentFile: "ai-usage-monitor-config.json"
        onAccepted: {
            var configData = {
                version: AppInfo.version,
                general: {
                    refreshInterval: plasmoid.configuration.refreshInterval,
                    compactDisplayMode: plasmoid.configuration.compactDisplayMode
                },
                openaiEnabled: plasmoid.configuration.openaiEnabled,
                openaiModel: plasmoid.configuration.openaiModel,
                anthropicEnabled: plasmoid.configuration.anthropicEnabled,
                anthropicModel: plasmoid.configuration.anthropicModel,
                googleEnabled: plasmoid.configuration.googleEnabled,
                googleModel: plasmoid.configuration.googleModel,
                ollamaEnabled: plasmoid.configuration.ollamaEnabled
            };
            var jsonStr = JSON.stringify(configData, null, 2);
            AppInfo.exportConfig(jsonStr, selectedFile.toString());
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
                    if (configData.general) {
                        if (configData.general.refreshInterval !== undefined) plasmoid.configuration.refreshInterval = configData.general.refreshInterval;
                        if (configData.general.compactDisplayMode !== undefined) plasmoid.configuration.compactDisplayMode = configData.general.compactDisplayMode;
                    }
                    if (configData.openaiEnabled !== undefined) plasmoid.configuration.openaiEnabled = configData.openaiEnabled;
                    if (configData.openaiModel !== undefined) plasmoid.configuration.openaiModel = configData.openaiModel;
                    if (configData.anthropicEnabled !== undefined) plasmoid.configuration.anthropicEnabled = configData.anthropicEnabled;
                    if (configData.anthropicModel !== undefined) plasmoid.configuration.anthropicModel = configData.anthropicModel;
                    if (configData.googleEnabled !== undefined) plasmoid.configuration.googleEnabled = configData.googleEnabled;
                    if (configData.googleModel !== undefined) plasmoid.configuration.googleModel = configData.googleModel;
                    if (configData.ollamaEnabled !== undefined) plasmoid.configuration.ollamaEnabled = configData.ollamaEnabled;
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

        QQC2.Label {
            Kirigami.FormData.label: i18n("Provider Health:")
            text: i18n("%1 providers enabled", enabledProviderCount())
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
                text: secrets.walletOpen ? i18n("Wallet is open and accessible") : i18n("Wallet is closed or inaccessible. API keys cannot be saved or loaded.")
                color: secrets.walletOpen ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
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
                text: syncDetector.hasCurrentBrowserProfile ? i18n("Found browser profiles for sync") : i18n("No supported browser profile found")
                color: syncDetector.hasCurrentBrowserProfile ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
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
            text: i18n("Export your configuration (excluding secrets) to a file. Safe to share.")
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
            text: AppInfo.version
        }

        QQC2.Button {
            Kirigami.FormData.label: i18n("Actions:")
            text: i18n("Run install_doctor.sh in terminal")
            icon.name: "utilities-terminal"
            onClicked: {
                Qt.openUrlExternally("konsole --hold -e sh -c 'cd " + AppInfo.pluginPath + "/../scripts && ./install_doctor.sh'");
            }
        }
    }

    function enabledProviderCount() {
        var keys = [
            "openaiEnabled", "anthropicEnabled", "googleEnabled",
            "mistralEnabled", "deepseekEnabled", "groqEnabled", "xaiEnabled",
            "ollamaEnabled", "openrouterEnabled", "togetherEnabled", "cohereEnabled",
            "googleveoEnabled", "azureEnabled", "bedrockEnabled"
        ];
        var count = 0;
        for (var i = 0; i < keys.length; i++) {
            if (plasmoid.configuration[keys[i]]) {
                count++;
            }
        }
        return count;
    }
}
