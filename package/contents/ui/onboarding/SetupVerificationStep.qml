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

    PlasmaExtras.Heading { level: 4; text: KI18n.i18n("Verify %1", step.controller.selectedSource.displayName || KI18n.i18n("source")) }
    PlasmaComponents.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: !step.controller.statusError
        visible: running
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: step.controller.statusError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        text: step.controller.statusMessage || KI18n.i18n("Running a read-only verification…")
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        opacity: 0.7
        text: KI18n.i18n("No inference request is sent by guided setup.")
    }
    RowLayout {
        visible: step.controller.statusError
        Layout.alignment: Qt.AlignHCenter
        PlasmaComponents.Button { text: KI18n.i18n("Back"); onClicked: step.controller.back() }
        PlasmaComponents.Button { text: KI18n.i18n("Try again"); onClicked: step.controller.retryVerification() }
    }
}
