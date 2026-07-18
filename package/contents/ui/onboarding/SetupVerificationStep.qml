import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    required property var controller
    spacing: Kirigami.Units.mediumSpacing

    PlasmaExtras.Heading { level: 4; text: i18n("Verify %1", controller.selectedSource.displayName || i18n("source")) }
    PlasmaComponents.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: !controller.statusError
        visible: running
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: controller.statusError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        text: controller.statusMessage || i18n("Running a read-only verification…")
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        opacity: 0.7
        text: i18n("No inference request is sent by guided setup.")
    }
    RowLayout {
        visible: controller.statusError
        Layout.alignment: Qt.AlignHCenter
        PlasmaComponents.Button { text: i18n("Back"); onClicked: controller.back() }
        PlasmaComponents.Button { text: i18n("Try again"); onClicked: controller.retryVerification() }
    }
}
