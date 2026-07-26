pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: alertsPage
    signal configurationChanged()

    property alias cfg_alertsEnabled: alertsSwitch.checked
    property alias cfg_warningThreshold: warningSlider.value
    property alias cfg_criticalThreshold: criticalSlider.value
    property alias cfg_notifyOnError: errorNotifySwitch.checked
    property alias cfg_notifyOnBudgetWarning: budgetNotifySwitch.checked
    property alias cfg_notifyOnDisconnect: disconnectNotifySwitch.checked
    property alias cfg_notifyOnReconnect: reconnectNotifySwitch.checked
    property alias cfg_notificationCooldownMinutes: cooldownSlider.value
    property alias cfg_notifyOnUpdate: updateNotifySwitch.checked
    property alias cfg_updateCheckInterval: updateCheckSpinBox.value

    property bool cfg_openaiNotificationsEnabled: Plasmoid.configuration.openaiNotificationsEnabled
    property bool cfg_anthropicNotificationsEnabled: Plasmoid.configuration.anthropicNotificationsEnabled
    property bool cfg_googleNotificationsEnabled: Plasmoid.configuration.googleNotificationsEnabled
    property bool cfg_mistralNotificationsEnabled: Plasmoid.configuration.mistralNotificationsEnabled
    property bool cfg_deepseekNotificationsEnabled: Plasmoid.configuration.deepseekNotificationsEnabled
    property bool cfg_groqNotificationsEnabled: Plasmoid.configuration.groqNotificationsEnabled
    property bool cfg_xaiNotificationsEnabled: Plasmoid.configuration.xaiNotificationsEnabled
    property bool cfg_ollamaNotificationsEnabled: Plasmoid.configuration.ollamaNotificationsEnabled
    property bool cfg_openrouterNotificationsEnabled: Plasmoid.configuration.openrouterNotificationsEnabled
    property bool cfg_togetherNotificationsEnabled: Plasmoid.configuration.togetherNotificationsEnabled
    property bool cfg_cohereNotificationsEnabled: Plasmoid.configuration.cohereNotificationsEnabled
    property bool cfg_googleveoNotificationsEnabled: Plasmoid.configuration.googleveoNotificationsEnabled
    property bool cfg_azureNotificationsEnabled: Plasmoid.configuration.azureNotificationsEnabled
    property bool cfg_bedrockNotificationsEnabled: Plasmoid.configuration.bedrockNotificationsEnabled
    property bool cfg_litellmNotificationsEnabled: Plasmoid.configuration.litellmNotificationsEnabled
    property bool cfg_cerebrasNotificationsEnabled: Plasmoid.configuration.cerebrasNotificationsEnabled
    property bool cfg_fireworksNotificationsEnabled: Plasmoid.configuration.fireworksNotificationsEnabled
    property bool cfg_perplexityNotificationsEnabled: Plasmoid.configuration.perplexityNotificationsEnabled
    property alias cfg_slackWebhookEnabled: slackWebhookSwitch.checked
    property alias cfg_discordWebhookEnabled: discordWebhookSwitch.checked
    property alias cfg_webhookCooldownMinutes: webhookCooldownSlider.value

    readonly property bool unsavedChanges: secretChanges.dirty
    property string secretStatusMessage: ""
    property bool secretStatusError: false

    // DND hours: config stores -1 (disabled) or 0-23 (hour).
    // ComboBox index: 0 = "Disabled", 1-24 = hours 0-23.
    property int cfg_dndStartHour: Plasmoid.configuration.dndStartHour
    property int cfg_dndEndHour: Plasmoid.configuration.dndEndHour

    property ProviderCatalog providerCatalog: ProviderCatalog {}

    SecretsManager {
        id: alertSecrets

        onWalletOpenChanged: {
            if (walletOpen && !secretChanges.dirty) {
                alertsPage.loadWebhookSecrets();
            }
        }
    }

    SecretChangeSet {
        id: secretChanges
        store: alertSecrets
    }

    function loadWebhookSecrets() {
        refreshWebhookSecret("slack_webhook_url", slackWebhookField);
        refreshWebhookSecret("discord_webhook_url", discordWebhookField);
    }

    function refreshWebhookSecret(key, field) {
        if (alertSecrets.hasKey(key)) {
            field.text = "********";
        } else {
            field.text = "";
        }
    }

    function stageSecret(key, value) {
        if (value.length > 0) secretChanges.stageStore(key, value);
        else secretChanges.stageRemove(key);
        secretStatusMessage = "";
        secretStatusError = false;
    }

    function saveConfig() {
        var result = secretChanges.commit();
        secretStatusError = !result.ok;
        if (result.ok) {
            secretStatusMessage = result.appliedKeys.length > 0
                ? i18n("Webhook credentials saved securely in KDE Wallet.") : "";
            if (result.appliedKeys.length > 0) loadWebhookSecrets();
        } else if (result.message === "wallet-not-open") {
            secretStatusMessage = i18n("KDE Wallet is not open. Unlock it and retry Apply.");
        } else {
            secretStatusMessage = i18n("Some webhook credentials could not be saved. Retry Apply.");
            if (result.appliedKeys.indexOf("slack_webhook_url") >= 0) {
                refreshWebhookSecret("slack_webhook_url", slackWebhookField);
            }
            if (result.appliedKeys.indexOf("discord_webhook_url") >= 0) {
                refreshWebhookSecret("discord_webhook_url", discordWebhookField);
            }
        }
        if (!result.ok) {
            Qt.callLater(function() { alertsPage.configurationChanged(); });
        }
    }

    function notificationEnabled(notificationsConfigKey) {
        return alertsPage["cfg_" + notificationsConfigKey];
    }

    function setNotificationEnabled(notificationsConfigKey, enabled) {
        alertsPage["cfg_" + notificationsConfigKey] = enabled;
    }

    function buildHourModel() {
        var items = [i18n("Disabled")];
        for (var h = 0; h < 24; h++) {
            items.push(h.toString().padStart(2, "0") + ":00");
        }
        return items;
    }

    Component.onCompleted: {
        if (alertSecrets.walletOpen) {
            loadWebhookSecrets();
        }
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        Kirigami.InlineMessage {
            visible: alertsPage.secretStatusMessage.length > 0
            text: alertsPage.secretStatusMessage
            type: alertsPage.secretStatusError ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            Layout.fillWidth: true
        }

        QQC2.Switch {
            id: alertsSwitch
            Kirigami.FormData.label: i18n("Enable alerts:")
            checked: Plasmoid.configuration.alertsEnabled
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Rate Limit Thresholds")
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Warning threshold:")
            enabled: alertsSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: warningSlider
                Layout.fillWidth: true
                from: 50
                to: 95
                stepSize: 5
                value: Plasmoid.configuration.warningThreshold

                onValueChanged: {
                    if (value >= criticalSlider.value) {
                        value = criticalSlider.value - 5;
                    }
                }
            }

            QQC2.Label {
                text: i18n("%1% of rate limit used", warningSlider.value)
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
            }

            QQC2.Label {
                text: i18n("Shows yellow warning indicator and optional notification")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Critical threshold:")
            enabled: alertsSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: criticalSlider
                Layout.fillWidth: true
                from: 60
                to: 100
                stepSize: 5
                value: Plasmoid.configuration.criticalThreshold

                onValueChanged: {
                    if (value <= warningSlider.value) {
                        value = warningSlider.value + 5;
                    }
                }
            }

            QQC2.Label {
                text: i18n("%1% of rate limit used", criticalSlider.value)
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
            }

            QQC2.Label {
                text: i18n("Shows red critical indicator and urgent notification")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Notification Types")
        }

        QQC2.Switch {
            id: errorNotifySwitch
            Kirigami.FormData.label: i18n("API errors:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.notifyOnError
        }

        QQC2.Switch {
            id: budgetNotifySwitch
            Kirigami.FormData.label: i18n("Budget warnings:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.notifyOnBudgetWarning
        }

        QQC2.Switch {
            id: disconnectNotifySwitch
            Kirigami.FormData.label: i18n("Provider disconnected:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.notifyOnDisconnect
        }

        QQC2.Switch {
            id: reconnectNotifySwitch
            Kirigami.FormData.label: i18n("Provider reconnected:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.notifyOnReconnect
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Per-Provider Notifications")
        }

        QQC2.Label {
            text: i18n("Disable notifications for specific providers. Global types above still apply.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            model: alertsPage.providerCatalog.providers

            QQC2.Switch {
                required property var modelData
                checked: alertsPage.notificationEnabled(modelData.notificationsConfigKey)
                Kirigami.FormData.label: modelData.label + ":"
                enabled: alertsSwitch.checked
                onToggled: alertsPage.setNotificationEnabled(modelData.notificationsConfigKey, checked)
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Update Notifications")
        }

        QQC2.Switch {
            id: updateNotifySwitch
            Kirigami.FormData.label: i18n("Notify on new version:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.notifyOnUpdate
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Check every:")
            enabled: alertsSwitch.checked && updateNotifySwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.SpinBox {
                id: updateCheckSpinBox
                from: 1
                to: 168
                stepSize: 1
                value: Plasmoid.configuration.updateCheckInterval
            }

            QQC2.Label {
                text: i18n("hours")
                color: Kirigami.Theme.disabledTextColor
            }
        }

        QQC2.Label {
            text: i18n("Checks GitHub for new releases and shows a KDE notification when an update is available.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            enabled: alertsSwitch.checked
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Webhooks")
        }

        QQC2.Switch {
            id: slackWebhookSwitch
            Kirigami.FormData.label: i18n("Slack:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.slackWebhookEnabled
        }

        QQC2.TextField {
            id: slackWebhookField
            Kirigami.FormData.label: i18n("Slack URL:")
            enabled: alertsSwitch.checked && slackWebhookSwitch.checked && alertSecrets.walletOpen
            echoMode: TextInput.Password
            placeholderText: i18n("Stored in KWallet")
            Layout.fillWidth: true
            onTextEdited: alertsPage.stageSecret("slack_webhook_url", text)
        }

        QQC2.Switch {
            id: discordWebhookSwitch
            Kirigami.FormData.label: i18n("Discord:")
            enabled: alertsSwitch.checked
            checked: Plasmoid.configuration.discordWebhookEnabled
        }

        QQC2.TextField {
            id: discordWebhookField
            Kirigami.FormData.label: i18n("Discord URL:")
            enabled: alertsSwitch.checked && discordWebhookSwitch.checked && alertSecrets.walletOpen
            echoMode: TextInput.Password
            placeholderText: i18n("Stored in KWallet")
            Layout.fillWidth: true
            onTextEdited: alertsPage.stageSecret("discord_webhook_url", text)
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Webhook cooldown:")
            enabled: alertsSwitch.checked && (slackWebhookSwitch.checked || discordWebhookSwitch.checked)
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: webhookCooldownSlider
                Layout.fillWidth: true
                from: 1
                to: 60
                stepSize: 1
                value: Plasmoid.configuration.webhookCooldownMinutes
            }

            QQC2.Label {
                text: i18n("%1 minutes", webhookCooldownSlider.value)
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Cooldown & Do Not Disturb")
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Notification cooldown:")
            enabled: alertsSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: cooldownSlider
                Layout.fillWidth: true
                from: 1
                to: 60
                stepSize: 1
                value: Plasmoid.configuration.notificationCooldownMinutes
            }

            QQC2.Label {
                text: i18np("%1 minute between repeated notifications",
                            "%1 minutes between repeated notifications",
                            cooldownSlider.value)
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Do Not Disturb:")
            enabled: alertsSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.ComboBox {
                id: dndStartCombo
                model: alertsPage.buildHourModel()
                currentIndex: alertsPage.cfg_dndStartHour >= 0 ? alertsPage.cfg_dndStartHour + 1 : 0
                onCurrentIndexChanged: alertsPage.cfg_dndStartHour = currentIndex === 0 ? -1 : currentIndex - 1
            }

            QQC2.Label {
                text: i18n("to")
            }

            QQC2.ComboBox {
                id: dndEndCombo
                enabled: dndStartCombo.currentIndex > 0
                model: alertsPage.buildHourModel()
                currentIndex: alertsPage.cfg_dndEndHour >= 0 ? alertsPage.cfg_dndEndHour + 1 : 0
                onCurrentIndexChanged: alertsPage.cfg_dndEndHour = currentIndex === 0 ? -1 : currentIndex - 1
            }
        }

        QQC2.Label {
            enabled: alertsSwitch.checked
            text: i18n("Suppress all notifications during this time window. Set start to 'Disabled' to turn off DND.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Preview")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Status colors:")
            spacing: Kirigami.Units.largeSpacing

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Rectangle { implicitWidth: 12; implicitHeight: 12; radius: 6; color: Kirigami.Theme.positiveTextColor }
                QQC2.Label { text: i18n("OK"); font.pointSize: Kirigami.Theme.smallFont.pointSize }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Rectangle { implicitWidth: 12; implicitHeight: 12; radius: 6; color: Kirigami.Theme.neutralTextColor }
                QQC2.Label { text: i18n("Warning"); font.pointSize: Kirigami.Theme.smallFont.pointSize }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Rectangle { implicitWidth: 12; implicitHeight: 12; radius: 6; color: Kirigami.Theme.negativeTextColor }
                QQC2.Label { text: i18n("Critical"); font.pointSize: Kirigami.Theme.smallFont.pointSize }
            }
        }
    }
}
