pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root

    // Plasma's KPluginMetaData value type is absent from its installed qmltypes.
    // qmllint disable unresolved-type
    readonly property string frontendVersion:
        Plasmoid["metaData"]?.["version"] || i18n("Unknown")
    // qmllint enable unresolved-type
    readonly property var nativeRoot: dependencyController.runtimeItem
    readonly property bool runtimeReady: dependencyController.loadState === DependencyBootstrapController.Ready

    switchWidth: Kirigami.Units.gridUnit * 12
    switchHeight: Kirigami.Units.gridUnit * 12

    toolTipMainText: i18n("AI Usage Monitor")
    toolTipSubText: root.runtimeReady && root.nativeRoot
                        ? root.nativeRoot.toolTipSubText
                        : i18n("Native plugin setup required")

    compactRepresentation: adaptiveCompactRepresentation
    fullRepresentation: adaptiveFullRepresentation

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
            // The runtime plasmoid object exposes activated(), but QObject's
            // tooling metadata cannot describe that injected instance.
            // qmllint disable missing-property
            onClicked: root.plasmoid["activated"]()
            // qmllint enable missing-property

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
        id: adaptiveCompactRepresentation

        RuntimeRepresentationLoader {
            runtimeReady: root.runtimeReady && root.nativeRoot !== null
            bootstrapRepresentation: bootstrapCompactRepresentation
            runtimeRepresentation: root.nativeRoot
                ? root.nativeRoot.compactRepresentationComponent : null
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

    Component {
        id: adaptiveFullRepresentation

        RuntimeRepresentationLoader {
            runtimeReady: root.runtimeReady && root.nativeRoot !== null
            bootstrapRepresentation: bootstrapFullRepresentation
            runtimeRepresentation: root.nativeRoot
                ? root.nativeRoot.fullRepresentationComponent : null
        }
    }
}
