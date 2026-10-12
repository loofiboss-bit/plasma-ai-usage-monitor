pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import "components" as Components

Kirigami.FormLayout {
    id: details

    required property var descriptor
    required property var controller
    required property var configPage

    readonly property bool hasSource: !!descriptor.configKey
    readonly property bool sourceEnabled: hasSource
        && !!controller.value(descriptor.enabledConfigKey)
    readonly property bool advancedSettingsExpanded: advancedSettings.expanded
    readonly property bool isCopilot: descriptor.configKey === "github-copilot"
    readonly property bool isAntigravity: descriptor.configKey === "google-antigravity"

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.maximumWidth: Kirigami.Units.gridUnit * 32
    Layout.alignment: Qt.AlignLeft
    wideMode: false

    Connections {
        target: details.configPage
        function onSecretCommitRevisionChanged() {
            if (details.isCopilot) copilotCredential.text = "";
        }
    }

    Kirigami.Heading {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: ""
        text: details.hasSource ? details.descriptor.name : i18n("Select a source")
        level: 2
    }
    QQC2.Label {
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Installation:")
        text: details.descriptor.installed ? i18n("Detected") : i18n("Not installed")
        color: details.descriptor.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
    }
    QQC2.Label {
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Usage data:")
        text: details.descriptor.monitoringLevel === "actual_quota"
            ? i18n("Live quota") : i18n("Local activity estimate")
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    QQC2.Switch {
        objectName: "sourceEnabledSwitch"
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Enabled:")
        checked: details.sourceEnabled
        text: i18n("Include activity in the overview and alerts")
        Accessible.name: i18n("Enable monitoring for %1", details.descriptor.name || i18n("source"))
        onToggled: details.controller.setValue(details.descriptor.enabledConfigKey, checked)
    }

    QQC2.Label {
        visible: details.isCopilot
        Kirigami.FormData.label: i18n("Connection:")
        text: i18n("Optional GitHub organization token for Copilot seat metrics. Requires the manage_billing:copilot permission.")
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    QQC2.Label {
        visible: details.isCopilot && !details.configPage.walletOpen
        text: i18n("Unlock KDE Wallet to edit credentials.")
        color: Kirigami.Theme.negativeTextColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    CredentialEditor {
        id: copilotCredential
        visible: details.isCopilot
        Kirigami.FormData.label: i18n("GitHub token:")
        label: i18n("GitHub token")
        editable: details.sourceEnabled && details.configPage.walletOpen
        clearEnabled: details.configPage.hasStoredOrPendingSecret("copilot_github")
        placeholderText: details.configPage.optionalSecretPlaceholder("copilot_github")
        onCredentialEdited: function(value) {
            details.configPage.stageSecret("copilot_github", value);
        }
        onClearRequested: details.configPage.removeSecret("copilot_github")
    }

    QQC2.Label {
        visible: details.hasSource && !details.isCopilot && !details.isAntigravity
        Kirigami.FormData.label: i18n("Data source:")
        text: i18n("Local activity from the selected application")
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.Label {
        visible: details.isAntigravity
        Kirigami.FormData.label: i18n("Detected plan:")
        text: details.configPage.sourceDetectedPlan(details.descriptor.configKey)
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    QQC2.Label {
        visible: details.isAntigravity
        Kirigami.FormData.label: i18n("Connection:")
        text: details.configPage.sourceConnectionState(details.descriptor.configKey)
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    QQC2.Label {
        visible: details.isAntigravity
        Kirigami.FormData.label: i18n("Last successful refresh:")
        text: details.configPage.sourceLastSuccessfulRefresh(details.descriptor.configKey)
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    QQC2.Label {
        visible: details.isAntigravity
        Kirigami.FormData.label: i18n("Quota status:")
        text: details.configPage.sourceQuotaStatus(details.descriptor.configKey)
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
        Layout.fillWidth: true
    }
    QQC2.ComboBox {
        id: planBox
        objectName: "sourcePlanSelector"
        visible: details.hasSource && !!details.descriptor.modelConfigKey
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Plan:")
        model: details.hasSource && details.descriptor.monitor.availablePlans
            ? details.descriptor.monitor.availablePlans() : []
        currentIndex: {
            var plans = model;
            var current = String(details.controller.value(details.descriptor.modelConfigKey) || "");
            for (var i = 0; i < plans.length; ++i) {
                if (details.descriptor.monitor.planIdForLabel
                        && details.descriptor.monitor.planIdForLabel(plans[i]) === current) return i;
            }
            return 0;
        }
        onActivated: {
            if (details.descriptor.monitor.planIdForLabel) {
                details.controller.setValue(details.descriptor.modelConfigKey,
                    details.descriptor.monitor.planIdForLabel(currentText));
                if (details.descriptor.legacyPlanConfigKey)
                    details.controller.setValue(details.descriptor.legacyPlanConfigKey, currentIndex);
            }
        }
    }

    QQC2.ComboBox {
        objectName: "sourceBillingModeSelector"
        visible: details.hasSource && !!details.descriptor.billingModeConfigKey
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Billing mode:")
        model: [i18n("Automatic"), i18n("Premium requests"), i18n("AI credits")]
        currentIndex: {
            var mode = String(details.controller.value(details.descriptor.billingModeConfigKey) || "auto");
            if (mode === "premium_requests_legacy" || mode === "premium_requests") return 1;
            if (mode === "ai_credits_usage_based" || mode === "usage_based" || mode === "credits") return 2;
            return 0;
        }
        onActivated: details.controller.setValue(details.descriptor.billingModeConfigKey,
            currentIndex === 1 ? "premium_requests_legacy" : currentIndex === 2 ? "ai_credits_usage_based" : "auto")
    }

    QQC2.Switch {
        visible: details.hasSource && !!details.descriptor.notificationsConfigKey
        objectName: "sourceQuotaWarningSwitch"
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Quota alerts:")
        text: i18n("Warn when quota is low")
        checked: details.hasSource && !!details.controller.value(details.descriptor.notificationsConfigKey)
        onToggled: details.controller.setValue(details.descriptor.notificationsConfigKey, checked)
    }

    ProviderVerificationResult {
        Kirigami.FormData.isSection: true
        sourceId: details.descriptor.configKey || ""
        resultSourceId: details.configPage.verificationSourceId
        stateKey: details.configPage.verificationState
        message: details.configPage.verificationMessage
        timestamp: details.configPage.verificationTimestamp
    }
    RowLayout {
        visible: details.hasSource
        Kirigami.FormData.isSection: true
        Layout.fillWidth: true
        QQC2.Label {
            Layout.fillWidth: true
            visible: details.configPage.hasUnsavedChanges
            text: i18n("Apply pending changes before verification.")
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }
        QQC2.Button {
            objectName: "sourceVerificationButton"
            text: i18n("Verify")
            icon.name: "security-high"
            enabled: details.sourceEnabled && !details.configPage.hasUnsavedChanges
            Accessible.name: i18n("Verify %1 using its read-only check", details.descriptor.name)
            onClicked: details.configPage.requestVerification(details.descriptor.configKey)
        }
        QQC2.Button {
            objectName: "sourceQuotaRefreshButton"
            visible: details.isAntigravity
            enabled: details.sourceEnabled && !details.configPage.hasUnsavedChanges
            text: i18n("Refresh quota")
            icon.name: "view-refresh"
            Accessible.name: i18n("Refresh Google Antigravity quota now")
            onClicked: details.configPage.refreshSourceQuota(details.descriptor.configKey)
        }
    }

    Components.CollapsibleSection {
        id: advancedSettings
        objectName: "sourceAdvancedSettings"
        Kirigami.FormData.isSection: true
        visible: details.hasSource && details.configPage.advancedMode
        Layout.fillWidth: true
        title: i18n("Advanced settings")
        summaryText: details.advancedSettingsExpanded ? "" : i18n("Optional source overrides")
    }

    QQC2.TextField {
        visible: details.configPage.advancedMode && details.advancedSettingsExpanded
                 && !!details.descriptor.orgNameConfigKey
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Organization name:")
        Accessible.name: i18n("Organization name")
        placeholderText: i18n("Organization name")
        text: details.hasSource
            ? String(details.controller.value(details.descriptor.orgNameConfigKey) || "") : ""
        onTextEdited: details.controller.setValue(details.descriptor.orgNameConfigKey, text)
    }
    QQC2.SpinBox {
        visible: details.configPage.advancedMode && details.advancedSettingsExpanded
                 && !!details.descriptor.resetDayConfigKey
        objectName: "monthlyResetDaySelector"
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Monthly reset day:")
        Accessible.name: i18n("Monthly reset day")
        from: 1
        to: 31
        value: details.hasSource ? Number(details.controller.value(details.descriptor.resetDayConfigKey) || 1) : 1
        onValueModified: details.controller.setValue(details.descriptor.resetDayConfigKey, value)
    }
    QQC2.SpinBox {
        visible: details.configPage.advancedMode && details.advancedSettingsExpanded
                 && !!details.descriptor.customLimitConfigKey
        objectName: "customQuotaLimitSelector"
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Custom quota limit:")
        Accessible.name: i18n("Custom quota limit")
        from: 0
        to: 999999
        editable: true
        value: details.hasSource ? Number(details.controller.value(details.descriptor.customLimitConfigKey) || 0) : 0
        onValueModified: details.controller.setValue(details.descriptor.customLimitConfigKey, value)
    }
    QQC2.SpinBox {
        visible: details.configPage.advancedMode && details.advancedSettingsExpanded
                 && !!details.descriptor.refreshIntervalConfigKey
        enabled: details.sourceEnabled
        Kirigami.FormData.label: i18n("Refresh interval:")
        Accessible.name: i18n("Refresh interval")
        from: 60
        to: 3600
        stepSize: 60
        editable: true
        value: details.hasSource ? Number(details.controller.value(details.descriptor.refreshIntervalConfigKey) || 300) : 300
        textFromValue: function(value) { return i18n("%1 seconds", value); }
        onValueModified: details.controller.setValue(details.descriptor.refreshIntervalConfigKey, value)
    }
}
