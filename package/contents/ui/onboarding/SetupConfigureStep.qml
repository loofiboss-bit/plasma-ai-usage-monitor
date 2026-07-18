import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import ".." as AppUi

ColumnLayout {
    required property var controller
    spacing: Kirigami.Units.mediumSpacing

    PlasmaExtras.Heading {
        level: 4
        text: i18n("Set up %1", controller.selectedSource.displayName || i18n("source"))
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: controller.monitoringLevelLabel(controller.selectedSource)
        font.bold: true
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: i18n("Expected result: %1", controller.qualityLabel(controller.selectedSource))
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: controller.selectedSource.sourceKindKey === "local_tool"
            ? i18n("No credential is needed. The verification reads local activity metadata and never sends an inference request.")
            : i18n("Only fields required for this source are shown. Credentials are saved explicitly in KDE Wallet.")
    }

    Repeater {
        model: controller.requiredCredentialSlots

        AppUi.CredentialEditor {
            required property string modelData
            Kirigami.FormData.label: controller.credentialLabel(modelData) + ":"
            Layout.fillWidth: true
            label: controller.credentialLabel(modelData)
            editable: controller.secretStore && controller.secretStore.walletOpen
            showClearAction: false
            placeholderText: controller.hasStoredCredential(modelData)
                ? i18n("Saved in KDE Wallet — leave blank to keep")
                : i18n("Required")
            onCredentialEdited: function(value) { controller.setCredential(modelData, value); }
        }
    }

    ColumnLayout {
        visible: controller.needsCustomEndpoint
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents.Label { text: i18n("Endpoint URL") }
        PlasmaComponents.TextField {
            Layout.fillWidth: true
            inputMethodHints: Qt.ImhUrlCharactersOnly
            placeholderText: i18n("https://gateway.example.com")
            Accessible.name: i18n("Required endpoint URL")
            onTextChanged: controller.customEndpoint = text
        }
    }

    PlasmaComponents.Label {
        visible: controller.statusMessage.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: controller.statusError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
        text: controller.statusMessage
    }

    RowLayout {
        Layout.fillWidth: true
        PlasmaComponents.Button { text: i18n("Back"); onClicked: controller.back() }
        Item { Layout.fillWidth: true }
        PlasmaComponents.Button {
            text: controller.selectedSource.sourceKindKey === "provider"
                ? i18n("Save and verify") : i18n("Enable and verify")
            icon.name: "security-high"
            onClicked: controller.saveAndVerify()
        }
    }
}
