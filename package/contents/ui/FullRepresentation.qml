pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
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
    property bool sourceDetailVisible: AppInfo.smokeView === "source-detail"
        || AppInfo.smokeView === "media-source-detail"
    property string detailSourceId: sourceDetailVisible ? "openrouter" : ""
    property string returnFocusSourceId: ""
    property string historySourceId: ""
    property string historyMetric: ""
    property int historyRangeDays: 7

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
                text: i18n("AI Usage Monitor")
                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Kirigami.NavigationTabBar {
            id: navigationTabs
            Layout.fillWidth: true
            visible: !fullRoot.showGuidedSetup
            actions: [
                Kirigami.Action {
                    text: i18n("Overview")
                    icon.name: "view-dashboard"
                    checkable: true
                    checked: !fullRoot.sourceDetailVisible
                        && fullRoot.destination === 0
                    onTriggered: {
                        fullRoot.sourceDetailVisible = false;
                        fullRoot.destination = 0;
                    }
                },
                Kirigami.Action {
                    text: i18n("History")
                    icon.name: "view-history"
                    checkable: true
                    checked: !fullRoot.sourceDetailVisible
                        && fullRoot.destination === 1
                    onTriggered: {
                        fullRoot.sourceDetailVisible = false;
                        fullRoot.destination = 1;
                    }
                },
                Kirigami.Action {
                    text: i18n("Insights")
                    icon.name: "office-chart-line"
                    checkable: true
                    checked: !fullRoot.sourceDetailVisible
                        && fullRoot.destination === 2
                    onTriggered: {
                        fullRoot.sourceDetailVisible = false;
                        fullRoot.destination = 2;
                    }
                }
            ]
        }

        QQC2.ToolBar {
            Layout.fillWidth: true
            visible: !fullRoot.showGuidedSetup
            Accessible.name: i18n("Monitor actions")

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.rightMargin: Kirigami.Units.smallSpacing
                Item { Layout.fillWidth: true }
                PlasmaComponents.ToolButton {
                    activeFocusOnTab: true
                    icon.name: "view-refresh"
                    text: i18n("Refresh")
                    display: QQC2.AbstractButton.TextBesideIcon
                    Accessible.name: i18n("Refresh all configured sources")
                    onClicked: fullRoot.monitor.refreshAll()
                    PlasmaComponents.ToolTip {
                        text: i18n("Refresh all configured sources")
                    }
                }
                PlasmaComponents.ToolButton {
                    activeFocusOnTab: true
                    icon.name: "tools-wizard"
                    Accessible.name: i18n("Run guided setup again")
                    onClicked: onboardingFlow.startAgain()
                    PlasmaComponents.ToolTip {
                        text: i18n("Run guided setup again")
                    }
                }
                PlasmaComponents.ToolButton {
                    activeFocusOnTab: true
                    icon.name: "configure"
                    Accessible.name: i18n("Configure AI Usage Monitor")
                    onClicked: Plasmoid.internalAction("configure").trigger()
                    PlasmaComponents.ToolTip { text: i18n("Configure") }
                }
            }
        }

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
                text: i18n("Setup was skipped. No source has been verified yet.")
            }
            PlasmaComponents.Button {
                text: i18n("Resume setup")
                activeFocusOnTab: true
                Accessible.name: i18n("Resume guided setup")
                onClicked: onboardingFlow.resume()
            }
        }

        Loader {
            id: destinationLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: !fullRoot.showGuidedSetup
            asynchronous: fullRoot.destination !== 0
            source: fullRoot.sourceDetailVisible ? "views/SourceDetailView.qml"
                  : fullRoot.destination === 0 ? "views/OverviewView.qml"
                  : fullRoot.destination === 1 ? "views/HistoryView.qml"
                  : "views/AnalystView.qml"
            onLoaded: {
                AppInfo.performanceMark("destination_component_loaded");
                item.monitor = fullRoot.monitor;
                if (fullRoot.sourceDetailVisible) {
                    item.sourceId = fullRoot.detailSourceId;
                } else if (fullRoot.destination === 1) {
                    item.requestedSourceId = fullRoot.historySourceId;
                    item.requestedMetric = fullRoot.historyMetric;
                    item.requestedRangeDays = fullRoot.historyRangeDays;
                } else if (fullRoot.destination === 0
                           && fullRoot.returnFocusSourceId !== "") {
                    var sourceId = fullRoot.returnFocusSourceId;
                    fullRoot.returnFocusSourceId = "";
                    Qt.callLater(function() {
                        // qmllint disable missing-property
                        destinationLoader.item.restoreSourceFocus(sourceId);
                        // qmllint enable missing-property
                    });
                }
            }
        }

        Connections {
            target: destinationLoader.item
            enabled: destinationLoader.status === Loader.Ready
            ignoreUnknownSignals: true

            function onSourceRequested(stableId) {
                fullRoot.detailSourceId = stableId;
                fullRoot.returnFocusSourceId = stableId;
                fullRoot.sourceDetailVisible = true;
            }
            function onBackRequested() {
                fullRoot.sourceDetailVisible = false;
                fullRoot.destination = 0;
            }
            function onActionRequested(stableId, actionKey, sourceKind) {
                fullRoot.monitor.fixOverviewSource(stableId, actionKey,
                                                   sourceKind);
            }
            function onSettingsRequested(stableId) {
                fullRoot.monitor.fixOverviewSource(
                    stableId, "open_source_settings", "");
            }
            function onHistoryRequested(historyId, metric, rangeDays) {
                fullRoot.historySourceId = historyId;
                fullRoot.historyMetric = metric;
                fullRoot.historyRangeDays = rangeDays;
                fullRoot.sourceDetailVisible = false;
                fullRoot.destination = 1;
            }
        }
    }

    Component.onCompleted: {
        AppInfo.performanceMark("full_representation_created");
        Qt.callLater(function() {
            AppInfo.performanceMark("first_rendered_frame");
        });
        if (AppInfo.smokeView === "settings") {
            Qt.callLater(function() {
                var configureAction = Plasmoid.internalAction("configure");
                if (configureAction) configureAction.trigger();
            });
        }
    }
}
