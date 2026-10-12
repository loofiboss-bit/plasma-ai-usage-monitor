pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import "components" as Components

Components.CollapsibleSection {
    id: browserSyncSettings

    required property var controller
    required property var configPage

    title: i18n("Browser sync")
    summaryText: i18n("Read-only sessions from a supported browser")

    QQC2.Label {
        Layout.fillWidth: true
        text: i18n("Browser cookies are read-only. Credentials are never stored by the widget.")
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true
        wideMode: false

        QQC2.Switch {
            objectName: "browserSyncEnabledSwitch"
            Kirigami.FormData.label: i18n("Session sync:")
            checked: browserSyncSettings.controller.value("browserSyncEnabled")
            text: i18n("Read browser sessions for usage data")
            Accessible.name: i18n("Sync supported browser sessions")
            onToggled: browserSyncSettings.controller.setValue("browserSyncEnabled", checked)
        }

        QQC2.ComboBox {
            objectName: "browserSyncBrowserSelector"
            Kirigami.FormData.label: i18n("Browser:")
            enabled: browserSyncSettings.controller.value("browserSyncEnabled")
            model: [i18n("Firefox"), i18n("Chrome"), i18n("Chromium"), i18n("Brave")]
            currentIndex: Math.max(0, Number(browserSyncSettings.controller.value("browserSyncBrowser") || 0))
            Accessible.name: i18n("Browser for session sync")
            onActivated: {
                browserSyncSettings.controller.setValue("browserSyncBrowser", currentIndex);
                browserSyncSettings.configPage.reloadBrowserProfiles();
            }
        }

        QQC2.ComboBox {
            objectName: "browserSyncProfileSelector"
            Kirigami.FormData.label: i18n("Profile:")
            enabled: browserSyncSettings.controller.value("browserSyncEnabled")
            model: browserSyncSettings.configPage.browserProfileModel
            currentIndex: {
                var index = browserSyncSettings.configPage.browserProfileModel.indexOf(
                    browserSyncSettings.controller.value("browserSyncProfile"));
                return index < 0 ? 0 : index;
            }
            Accessible.name: i18n("Browser profile for session sync")
            onActivated: {
                browserSyncSettings.controller.setValue("browserSyncProfile",
                    currentIndex > 0 ? currentText : "");
                browserSyncSettings.configPage.browserTestResults = ({});
            }
        }

        QQC2.SpinBox {
            objectName: "browserSyncIntervalSelector"
            Kirigami.FormData.label: i18n("Refresh interval:")
            enabled: browserSyncSettings.controller.value("browserSyncEnabled")
            from: 60
            to: 3600
            stepSize: 60
            value: Number(browserSyncSettings.controller.value("browserSyncInterval") || 300)
            Accessible.name: i18n("Browser session refresh interval")
            textFromValue: function(value) { return i18n("%1 seconds", value); }
            onValueModified: browserSyncSettings.controller.setValue("browserSyncInterval", value)
        }

        QQC2.Button {
            objectName: "browserSyncReloadProfilesButton"
            Kirigami.FormData.label: i18n("Profiles:")
            text: i18n("Reload browser profiles")
            icon.name: "view-refresh"
            enabled: browserSyncSettings.controller.value("browserSyncEnabled")
            onClicked: browserSyncSettings.configPage.reloadBrowserProfiles()
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Button {
            objectName: "browserSyncTestClaudeButton"
            text: i18n("Test Claude session")
            icon.name: "network-connect"
            enabled: browserSyncSettings.controller.value("browserSyncEnabled")
                && !browserSyncSettings.configPage.hasUnsavedChanges
            onClicked: browserSyncSettings.configPage.testBrowserSession("claude", "Claude.ai")
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: !!browserSyncSettings.configPage.browserTestResults.claude
            text: browserSyncSettings.configPage.browserTestResults.claude
                ? browserSyncSettings.configPage.browserTestResults.claude.message : ""
            color: browserSyncSettings.configPage.browserTestResults.claude
                ? browserSyncSettings.configPage.syncStatusColor(
                    browserSyncSettings.configPage.browserTestResults.claude.code)
                : Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: !!browserSyncSettings.configPage.browserTestResults.claude
        text: browserSyncSettings.configPage.browserTestResults.claude
            ? browserSyncSettings.configPage.browserTestResults.claude.guidance : ""
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Button {
            objectName: "browserSyncTestChatGPTButton"
            text: i18n("Test ChatGPT session")
            icon.name: "network-connect"
            enabled: browserSyncSettings.controller.value("browserSyncEnabled")
                && !browserSyncSettings.configPage.hasUnsavedChanges
            onClicked: browserSyncSettings.configPage.testBrowserSession("chatgpt", "ChatGPT")
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: !!browserSyncSettings.configPage.browserTestResults.chatgpt
            text: browserSyncSettings.configPage.browserTestResults.chatgpt
                ? browserSyncSettings.configPage.browserTestResults.chatgpt.message : ""
            color: browserSyncSettings.configPage.browserTestResults.chatgpt
                ? browserSyncSettings.configPage.syncStatusColor(
                    browserSyncSettings.configPage.browserTestResults.chatgpt.code)
                : Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: !!browserSyncSettings.configPage.browserTestResults.chatgpt
        text: browserSyncSettings.configPage.browserTestResults.chatgpt
            ? browserSyncSettings.configPage.browserTestResults.chatgpt.guidance : ""
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: browserSyncSettings.configPage.hasUnsavedChanges
        text: i18n("Apply pending changes before testing a browser session.")
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.neutralTextColor
    }
}
