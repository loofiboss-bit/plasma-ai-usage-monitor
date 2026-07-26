pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import ".." as AppUi

ColumnLayout {
    id: step
    required property var controller
    spacing: Kirigami.Units.mediumSpacing

    PlasmaExtras.Heading {
        level: 4
        text: i18n("Set up %1", step.controller.selectedSource.displayName || i18n("source"))
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: step.controller.monitoringLevelLabel(step.controller.selectedSource)
        font.bold: true
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: i18n("Expected result: %1", step.controller.qualityLabel(step.controller.selectedSource))
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: step.controller.selectedSource.sourceKindKey === "local_tool"
            ? i18n("No credential is needed. The verification reads local activity metadata and never sends an inference request.")
            : step.controller.selectedSource.stableId === "anthropic"
                ? i18n("Choose either a standard key for a connection check or an organization Admin key for actual usage and spend. Both remain separate in KDE Wallet.")
            : i18n("Only fields required for this source are shown. Credentials are saved explicitly in KDE Wallet.")
    }

    Repeater {
        model: step.controller.requiredCredentialSlots

        AppUi.CredentialEditor {
            id: credentialEditor
            required property string modelData
            Kirigami.FormData.label: step.controller.credentialLabel(
                                         credentialEditor.modelData) + ":"
            Layout.fillWidth: true
            label: step.controller.credentialLabel(credentialEditor.modelData)
            editable: step.controller.secretStore && step.controller.secretStore.walletOpen
            showClearAction: false
            placeholderText: step.controller.hasStoredCredential(
                                 credentialEditor.modelData)
                ? i18n("Saved in KDE Wallet — leave blank to keep")
                : step.controller.acceptsAnyCredentialSet
                    ? i18n("Configure either key")
                    : i18n("Required")
            onCredentialEdited: function(value) {
                step.controller.setCredential(credentialEditor.modelData, value);
            }
        }
    }

    ColumnLayout {
        visible: step.controller.needsCustomEndpoint
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents.Label { text: i18n("Endpoint URL") }
        PlasmaComponents.TextField {
            Layout.fillWidth: true
            inputMethodHints: Qt.ImhUrlCharactersOnly
            placeholderText: i18n("https://gateway.example.com")
            Accessible.name: i18n("Required endpoint URL")
            onTextChanged: step.controller.customEndpoint = text
        }
    }

    PlasmaComponents.Label {
        visible: step.controller.statusMessage.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: step.controller.statusError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
        text: step.controller.statusMessage
    }

    RowLayout {
        Layout.fillWidth: true
        PlasmaComponents.Button { text: i18n("Back"); onClicked: step.controller.back() }
        Item { Layout.fillWidth: true }
        PlasmaComponents.Button {
            text: step.controller.selectedSource.sourceKindKey === "provider"
                ? i18n("Save and verify") : i18n("Enable and verify")
            icon.name: "security-high"
            onClicked: step.controller.saveAndVerify()
        }
    }
}
