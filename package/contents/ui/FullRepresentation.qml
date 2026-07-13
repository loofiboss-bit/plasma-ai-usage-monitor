import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "onboarding" as Onboarding

PlasmaExtras.Representation {
    id: fullRoot

    implicitWidth: Kirigami.Units.gridUnit * 28
    implicitHeight: Kirigami.Units.gridUnit * 28
    property int destination: AppInfo.smokeView === "history" ? 1
                            : AppInfo.smokeView === "analyst" ? 2 : 0

    readonly property bool hasConfiguration: {
        var providers = root.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].enabled) return true;
        }
        var tools = root.allSubscriptionTools || [];
        for (var j = 0; j < tools.length; j++) {
            if (tools[j].enabled) return true;
        }
        return AppInfo.demoMode;
    }

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
            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "view-refresh"
                onClicked: root.refreshAll()
                PlasmaComponents.ToolTip { text: i18n("Refresh all configured providers") }
            }
            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "configure"
                onClicked: plasmoid.internalAction("configure").trigger()
                PlasmaComponents.ToolTip { text: i18n("Configure") }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing
            visible: fullRoot.hasConfiguration

            Repeater {
                model: [i18n("Overview"), i18n("History"), i18n("Analyst")]
                PlasmaComponents.ToolButton {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData
                    checked: fullRoot.destination === index
                    activeFocusOnTab: true
                    onClicked: fullRoot.destination = index
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true; visible: fullRoot.hasConfiguration }

        Onboarding.OnboardingFlow {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !fullRoot.hasConfiguration
        }

        Loader {
            id: destinationLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: fullRoot.hasConfiguration
            asynchronous: fullRoot.destination !== 0
            source: fullRoot.destination === 0 ? "views/OverviewView.qml"
                  : fullRoot.destination === 1 ? "views/HistoryView.qml"
                  : "views/AnalystView.qml"
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            visible: fullRoot.hasConfiguration
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.6
            text: {
                var dbSize = root.usageDb ? root.usageDb.databaseSize() : 0;
                var dbText = dbSize > 0 ? i18n(" · DB %1 KB", Math.round(dbSize / 1024)) : "";
                return i18n("%1 providers connected", root.connectedCount || 0) + dbText;
            }
        }
    }
}
