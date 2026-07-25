pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "onboarding" as Onboarding

PlasmaExtras.Representation {
    id: fullRoot

    required property var monitor
    implicitWidth: Kirigami.Units.gridUnit * 28
    implicitHeight: Kirigami.Units.gridUnit * 28
    property int destination: AppInfo.smokeView === "history"
                              || AppInfo.smokeView.indexOf("media-history") === 0 ? 1
                            : AppInfo.smokeView === "analyst"
                              || AppInfo.smokeView.indexOf("media-analyst") === 0 ? 2 : 0

    readonly property bool hasConfiguration: {
        if (AppInfo.smokeView.indexOf("onboarding") === 0) return false;
        var providers = fullRoot.monitor.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].enabled) return true;
        }
        var tools = fullRoot.monitor.allSubscriptionTools || [];
        for (var j = 0; j < tools.length; j++) {
            if (tools[j].enabled) return true;
        }
        return AppInfo.demoMode;
    }
    readonly property bool showGuidedSetup: Plasmoid.configuration.setupWizardInProgress
        || (!fullRoot.hasConfiguration
            && !Plasmoid.configuration.setupWizardCompleted
            && !Plasmoid.configuration.setupWizardDismissed)

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
                text: KI18n.i18n("AI Usage Monitor")
                Layout.fillWidth: true
            }
            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "view-refresh"
                Accessible.name: KI18n.i18n("Refresh all configured sources")
                onClicked: fullRoot.monitor.refreshAll()
                PlasmaComponents.ToolTip { text: KI18n.i18n("Refresh all configured sources") }
            }
            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "tools-wizard"
                Accessible.name: KI18n.i18n("Run guided setup again")
                onClicked: onboardingFlow.startAgain()
                PlasmaComponents.ToolTip { text: KI18n.i18n("Run guided setup again") }
            }
            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "configure"
                Accessible.name: KI18n.i18n("Configure AI Usage Monitor")
                onClicked: Plasmoid.internalAction("configure").trigger()
                PlasmaComponents.ToolTip { text: KI18n.i18n("Configure") }
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
            visible: !fullRoot.showGuidedSetup

            Repeater {
                model: [KI18n.i18n("Overview"), KI18n.i18n("History"), KI18n.i18n("Analyst")]
                PlasmaComponents.ToolButton {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData
                    checked: fullRoot.destination === index
                    activeFocusOnTab: true
                    Accessible.name: KI18n.i18n("Open %1", modelData)
                    onClicked: fullRoot.destination = index
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true; visible: !fullRoot.showGuidedSetup }

        Onboarding.OnboardingFlow {
            id: onboardingFlow
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: fullRoot.showGuidedSetup
            runtime: fullRoot.monitor
            readinessModel: fullRoot.monitor.sourceReadiness
            secretStore: fullRoot.monitor.secretsManager
            configuration: Plasmoid.configuration
            previewState: AppInfo.smokeView
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            visible: !fullRoot.showGuidedSetup
                  && !fullRoot.hasConfiguration
                  && Plasmoid.configuration.setupWizardDismissed

            PlasmaComponents.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: KI18n.i18n("Setup was skipped. No source has been verified yet.")
            }
            PlasmaComponents.Button {
                text: KI18n.i18n("Resume setup")
                activeFocusOnTab: true
                Accessible.name: KI18n.i18n("Resume guided setup")
                onClicked: onboardingFlow.resume()
            }
        }

        Loader {
            id: destinationLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: !fullRoot.showGuidedSetup
            asynchronous: fullRoot.destination !== 0
            source: fullRoot.destination === 0 ? "views/OverviewView.qml"
                  : fullRoot.destination === 1 ? "views/HistoryView.qml"
                  : "views/AnalystView.qml"
            onLoaded: item.monitor = fullRoot.monitor
        }

    }

    Component.onCompleted: {
        if (AppInfo.smokeView === "settings") {
            Qt.callLater(function() {
                var configureAction = Plasmoid.internalAction("configure");
                if (configureAction) configureAction.trigger();
            });
        }
    }
}
