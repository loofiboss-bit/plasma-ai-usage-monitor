pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root

    readonly property string frontendVersion: plasmoid.metaData?.version || i18n("Unknown")
    readonly property var nativeRoot: dependencyController.runtimeItem
    readonly property bool runtimeReady: dependencyController.loadState === DependencyBootstrapController.Ready

    switchWidth: Kirigami.Units.gridUnit * 12
    switchHeight: Kirigami.Units.gridUnit * 12

    toolTipMainText: i18n("AI Usage Monitor")
    toolTipSubText: root.runtimeReady && root.nativeRoot
                        ? root.nativeRoot.toolTipSubText
                        : i18n("Native plugin setup required")

    compactRepresentation: root.runtimeReady && root.nativeRoot
                           ? root.nativeRoot.compactRepresentationComponent
                           : bootstrapCompactRepresentation
    fullRepresentation: root.runtimeReady && root.nativeRoot
                        ? root.nativeRoot.fullRepresentationComponent
                        : bootstrapFullRepresentation

    DependencyBootstrapController {
        id: dependencyController
        frontendVersion: root.frontendVersion
        probeSource: Qt.resolvedUrl("NativePluginProbe.qml")
        runtimeSource: Qt.resolvedUrl("NativeMonitor.qml")
    }

    Component {
        id: bootstrapCompactRepresentation

        MouseArea {
            id: compactRoot
            hoverEnabled: true
            Accessible.role: Accessible.Button
            Accessible.name: i18n("AI Usage Monitor needs its native plugin")
            onClicked: plasmoid.activated()

            Kirigami.Icon {
                anchors.fill: parent
                source: Qt.resolvedUrl("../icons/logo.png")
                active: compactRoot.containsMouse
            }

            Kirigami.Icon {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: Math.max(Kirigami.Units.iconSizes.small, parent.width * 0.42)
                height: width
                source: "dialog-warning"
            }
        }
    }

    Component {
        id: bootstrapFullRepresentation

        DependencyBootstrap {
            frontendVersion: root.frontendVersion
            installedPluginVersion: dependencyController.installedPluginVersion
            bootstrapState: dependencyController.stateName
            supportReport: dependencyController.supportReport()
        }
    }
}
