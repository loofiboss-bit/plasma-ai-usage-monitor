pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: step
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
            model: step.controller.candidates

            PlasmaComponents.RadioButton {
                id: sourceButton
                required property var modelData
                Layout.fillWidth: true
                checked: step.controller.selectedSourceId
                         === sourceButton.modelData.stableId
                text: sourceButton.modelData.displayName + " — "
                      + step.controller.monitoringLevelLabel(
                          sourceButton.modelData)
                Accessible.description: sourceButton.modelData.nextActionText
                onClicked: step.controller.selectSource(
                    sourceButton.modelData.stableId, false)
            }
        }
    }

    PlasmaComponents.Label {
        visible: step.controller.candidates.length === 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: i18n("No matching source is available. Go back and choose another goal.")
    }

    RowLayout {
        Layout.fillWidth: true
        PlasmaComponents.Button { text: i18n("Back"); onClicked: step.controller.back() }
        Item { Layout.fillWidth: true }
        PlasmaComponents.Button {
            text: i18n("Continue")
            enabled: step.controller.selectedSourceId.length > 0
            onClicked: step.controller.selectSource(step.controller.selectedSourceId, true)
        }
    }
}
