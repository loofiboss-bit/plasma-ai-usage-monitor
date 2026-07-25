import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: step
    required property var controller
    spacing: Kirigami.Units.mediumSpacing

    Kirigami.Icon {
        Layout.alignment: Qt.AlignHCenter
        source: "checkmark"
        Layout.preferredWidth: Kirigami.Units.iconSizes.large
        Layout.preferredHeight: Kirigami.Units.iconSizes.large
    }
    PlasmaExtras.Heading {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        level: 4
        text: KI18n.i18n("%1 is ready", step.controller.selectedSource.displayName || KI18n.i18n("Source"))
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.bold: true
        text: step.controller.resultQuality
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: step.controller.resultSummary
    }
    PlasmaComponents.Button {
        Layout.alignment: Qt.AlignHCenter
        text: KI18n.i18n("Open dashboard")
        icon.name: "go-next"
        onClicked: step.controller.finish()
    }
}
