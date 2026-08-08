pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

PlasmaExtras.Representation {
    id: popupRoot

    required property var monitor
    implicitWidth: Kirigami.Units.gridUnit * 28
    implicitHeight: Kirigami.Units.gridUnit * 28

    header: PlasmaExtras.PlasmoidHeading {
        RowLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: Qt.resolvedUrl("../icons/logo.png")
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
            }
            PlasmaExtras.Heading {
                level: 3
                text: i18n("AI Usage Monitor")
                Layout.fillWidth: true
            }
        }
    }

    Loader {
        id: popupContent
        anchors.fill: parent
        asynchronous: true
        source: "FullRepresentation.qml"
        onLoaded: item.monitor = popupRoot.monitor
    }

    PlasmaComponents.Label {
        anchors.centerIn: parent
        visible: popupContent.status !== Loader.Ready
        text: i18n("Loading Overview…")
        Accessible.name: i18n("Overview is loading")
    }
}
