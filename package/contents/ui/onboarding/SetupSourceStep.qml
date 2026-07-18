import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    required property var controller
    spacing: Kirigami.Units.mediumSpacing

    PlasmaExtras.Heading { level: 4; text: i18n("Choose a source") }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: i18n("Recommended sources appear first. The monitoring level tells you what the result can prove.")
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: controller.candidates

            PlasmaComponents.RadioButton {
                required property var modelData
                Layout.fillWidth: true
                checked: controller.selectedSourceId === modelData.stableId
                text: modelData.displayName + " — " + controller.monitoringLevelLabel(modelData)
                Accessible.description: modelData.nextActionText
                onClicked: controller.selectSource(modelData.stableId, false)
            }
        }
    }

    PlasmaComponents.Label {
        visible: controller.candidates.length === 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: i18n("No matching source is available. Go back and choose another goal.")
    }

    RowLayout {
        Layout.fillWidth: true
        PlasmaComponents.Button { text: i18n("Back"); onClicked: controller.back() }
        Item { Layout.fillWidth: true }
        PlasmaComponents.Button {
            text: i18n("Continue")
            enabled: controller.selectedSourceId.length > 0
            onClicked: controller.selectSource(controller.selectedSourceId, true)
        }
    }
}
