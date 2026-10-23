pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "onboarding" as Onboarding

Item {
    id: fullRoot

    required property var monitor
    property int destination: AppInfo.smokeView === "history"
                              || AppInfo.smokeView.indexOf("media-history") === 0
                              || AppInfo.smokeView === "media-analyst-sessions" ? 1
                            : AppInfo.smokeView === "analyst"
                              || AppInfo.smokeView.indexOf("media-analyst") === 0 ? 2 : 0
    property bool sourceDetailVisible: AppInfo.smokeView === "source-detail"
        || AppInfo.smokeView === "media-source-detail"
        || AppInfo.smokeView === "media-coding-sessions"
    property string detailSourceId: sourceDetailVisible
        ? AppInfo.smokeView === "media-coding-sessions" ? "codex-cli" : "openrouter"
        : ""
    property string returnFocusSourceId: ""
    property string historySourceId: ""
    property string historyMetric: ""
    property int historyRangeDays: 7
    property var sessionDayReturnState: null
    property bool historyRequestPending: false
    property var overviewViewState: ({})
    property var historyViewState: ({})
    property var insightsViewState: ({})
    property bool historySessionsMode: AppInfo.demoMode
        && (AppInfo.smokeView === "media-history-sessions"
            || AppInfo.smokeView === "media-analyst-sessions")
    property string historySessionToolId: "codex-cli"
    property string historySessionProjectId: ""
    property var localSessionViewState: ({
        sourceDetail: ({ toolId: "codex-cli", rangeIndex: 1 }),
        history: ({ toolId: "codex-cli", rangeIndex: 1,
            modelFilter: AppInfo.demoMode && AppInfo.smokeView === "media-history-sessions"
                ? {modelId: "gpt-5-codex"} : ({}) }),
        analyst: ({
            analystMode: false,
            toolId: "codex-cli", rangeIndex: 1
        })
    })

    function startGuidedSetupAgain() {
        Plasmoid.configuration.setupWizardDismissed = false;
        Plasmoid.configuration.setupWizardInProgress = true;
        Plasmoid.configuration.setupWizardStep = 0;
        Plasmoid.configuration.setupWizardGoal = "";
        Plasmoid.configuration.setupWizardSourceId = "";
    }

    function resumeGuidedSetup() {
        Plasmoid.configuration.setupWizardDismissed = false;
        Plasmoid.configuration.setupWizardInProgress = true;
    }

    function captureCurrentViewState() {
        // Loader.item is a QObject in qmltypes; concrete destinations expose this transient interface.
        // qmllint disable missing-property
        if (sourceDetailVisible || destinationLoader.status !== Loader.Ready
                || !destinationLoader.item
                || typeof destinationLoader.item.captureViewState !== "function")
            return;
        var state = destinationLoader.item.captureViewState() || ({});
        if (destination === 0) overviewViewState = state;
        else if (destination === 1) historyViewState = state;
        else if (destination === 2) insightsViewState = state;
        // qmllint enable missing-property
    }

    function cloneViewState(value) {
        try { return JSON.parse(JSON.stringify(value || ({}))); }
        catch (error) { return ({}); }
    }

    function applyHistoryRoute(item) {
        // qmllint disable missing-property
        if (!item || typeof item.applyViewState !== "function") return false;
        item.sessionsMode = fullRoot.historySessionsMode;
        item.sessionToolId = fullRoot.historySessionToolId;
        item.sessionProjectId = fullRoot.historySessionProjectId;
        item.sessionState = fullRoot.localSessionViewState.history;
        item.viewState = fullRoot.historyViewState;
        if (fullRoot.historyRequestPending) {
            item.requestedSourceId = fullRoot.historySourceId;
            item.requestedMetric = fullRoot.historyMetric;
            item.requestedRangeDays = fullRoot.historyRangeDays;
            fullRoot.historyRequestPending = false;
            fullRoot.historySourceId = "";
            fullRoot.historyMetric = "";
        }
        // qmllint enable missing-property
        return true;
    }

    function syncLoadedHistoryRoute() {
        if (sourceDetailVisible || destination !== 1
                || destinationLoader.status !== Loader.Ready)
            return false;
        return applyHistoryRoute(destinationLoader.item);
    }

    function routeToSessionHistory(toolId, projectId, rangeDays,
                                  sessionId, pageOffset, modelFilter) {
        captureCurrentViewState();
        historyViewState = ({});
        historySessionsMode = true;
        historySessionToolId = String(toolId || "codex-cli");
        historySessionProjectId = String(projectId || "");
        historyRangeDays = rangeDays;
        historyRequestPending = false;
        var nextSessionState = Object.assign({}, localSessionViewState.history);
        nextSessionState.toolId = historySessionToolId;
        nextSessionState.projectId = historySessionProjectId;
        nextSessionState.rangeIndex = [1, 7, 30].indexOf(Number(rangeDays));
        if (nextSessionState.rangeIndex < 0) nextSessionState.rangeIndex = 1;
        nextSessionState.rangeMode = "preset";
        nextSessionState.customStartDate = "";
        nextSessionState.customEndDate = "";
        nextSessionState.dayDrilldown = false;
        nextSessionState.dayFocusDate = "";
        nextSessionState.dailyScrollY = 0;
        nextSessionState.sessionId = String(sessionId || "");
        nextSessionState.pageOffset = pageOffset;
        nextSessionState.modelFilter = modelFilter || ({});
        nextSessionState.historyDisplayIndex = 0;
        nextSessionState.projectSearch = "";
        nextSessionState.modelSearch = "";
        localSessionViewState = Object.assign({}, localSessionViewState,
            { history: nextSessionState });
        sessionDayReturnState = null;
        sourceDetailVisible = false;
        destination = 1;
        Qt.callLater(syncLoadedHistoryRoute);
    }

    function routeToSessionHistoryRange(selection) {
        if (!selection || !selection.startDate || !selection.endDate)
            return;
        captureCurrentViewState();
        sessionDayReturnState = {
            destination: destination,
            historyViewState: cloneViewState(historyViewState),
            insightsViewState: cloneViewState(insightsViewState),
            localSessionViewState: cloneViewState(localSessionViewState),
            historySessionsMode: historySessionsMode,
            historySessionToolId: historySessionToolId,
            historySessionProjectId: historySessionProjectId
        };
        historyViewState = ({});
        historySessionsMode = true;
        historySessionToolId = String(selection.toolId || "codex-cli");
        historySessionProjectId = String(selection.projectId || "");
        historyRangeDays = 1;
        historyRequestPending = false;
        var nextSessionState = cloneViewState(localSessionViewState.history);
        nextSessionState.toolId = historySessionToolId;
        nextSessionState.projectId = historySessionProjectId;
        nextSessionState.modelFilter = selection.modelFilter || ({});
        nextSessionState.rangeIndex = 3;
        nextSessionState.rangeMode = "custom";
        nextSessionState.customStartDate = String(selection.startDate);
        nextSessionState.customEndDate = String(selection.endDate);
        nextSessionState.historyDisplayIndex = 0;
        nextSessionState.pageOffset = 0;
        nextSessionState.sessionId = "";
        nextSessionState.projectSearch = "";
        nextSessionState.modelSearch = "";
        nextSessionState.dayDrilldown = selection.dayDrilldown === true;
        if (selection.dayDrilldown === true)
            nextSessionState.dayFocusDate = String(selection.startDate);
        localSessionViewState = Object.assign({}, localSessionViewState,
            { history: nextSessionState });
        sourceDetailVisible = false;
        destination = 1;
        Qt.callLater(syncLoadedHistoryRoute);
    }

    function returnToSessionDayOverview() {
        var saved = sessionDayReturnState;
        if (!saved) return;
        historyViewState = cloneViewState(saved.historyViewState);
        insightsViewState = cloneViewState(saved.insightsViewState);
        localSessionViewState = cloneViewState(saved.localSessionViewState);
        historySessionsMode = !!saved.historySessionsMode;
        historySessionToolId = String(saved.historySessionToolId || "codex-cli");
        historySessionProjectId = String(saved.historySessionProjectId || "");
        sessionDayReturnState = null;
        sourceDetailVisible = false;
        destination = Number(saved.destination) === 2 ? 2 : 1;
        Qt.callLater(syncLoadedHistoryRoute);
    }

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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        QQC2.ToolBar {
            Layout.fillWidth: true
            visible: !fullRoot.showGuidedSetup
            Accessible.name: i18n("Monitor navigation and actions")

            contentItem: GridLayout {
                columns: fullRoot.width >= Kirigami.Units.gridUnit * 36 ? 2 : 1
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: 0

                QQC2.TabBar {
                    objectName: "monitorNavigation"
                    Layout.fillWidth: fullRoot.width < Kirigami.Units.gridUnit * 36
                    spacing: 0
                    currentIndex: fullRoot.destination
                    Repeater {
                        model: [
                            { label: i18n("Overview"), icon: "view-dashboard" },
                            { label: i18n("History"), icon: "view-history" },
                            { label: i18n("Insights"), icon: "office-chart-line" }
                        ]
                        QQC2.TabButton {
                            id: destinationTab
                            required property var modelData
                            required property int index
                            objectName: "monitorDestination-" + index
                            Layout.fillWidth: fullRoot.width < Kirigami.Units.gridUnit * 36
                            Layout.preferredWidth: fullRoot.width >= Kirigami.Units.gridUnit * 36
                                ? Kirigami.Units.gridUnit * 7 : -1
                            Layout.minimumWidth: 0
                            text: modelData.label
                            icon.name: modelData.icon
                            display: fullRoot.width >= Kirigami.Units.gridUnit * 24
                                ? QQC2.AbstractButton.TextBesideIcon
                                : QQC2.AbstractButton.IconOnly
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: {
                                fullRoot.captureCurrentViewState();
                                fullRoot.sourceDetailVisible = false;
                                fullRoot.destination = index;
                            }
                            PlasmaComponents.ToolTip { text: destinationTab.modelData.label }
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Kirigami.Units.smallSpacing
                    PlasmaComponents.ToolButton {
                        activeFocusOnTab: true
                        icon.name: "view-refresh"
                        text: i18n("Refresh")
                        display: fullRoot.width >= Kirigami.Units.gridUnit * 22
                            ? QQC2.AbstractButton.TextBesideIcon
                            : QQC2.AbstractButton.IconOnly
                        Accessible.name: i18n("Refresh all configured sources")
                        onClicked: fullRoot.monitor.refreshAll()
                        PlasmaComponents.ToolTip {
                            text: i18n("Refresh all configured sources")
                        }
                    }
                    PlasmaComponents.ToolButton {
                        activeFocusOnTab: true
                        icon.name: "configure"
                        text: i18n("Settings")
                        display: fullRoot.width >= Kirigami.Units.gridUnit * 22
                            ? QQC2.AbstractButton.TextBesideIcon
                            : QQC2.AbstractButton.IconOnly
                        Accessible.name: i18n("Configure AI Usage Monitor")
                        onClicked: Plasmoid.internalAction("configure").trigger()
                        PlasmaComponents.ToolTip { text: i18n("Configure") }
                    }
                    PlasmaComponents.ToolButton {
                        id: moreButton
                        activeFocusOnTab: true
                        icon.name: "application-menu"
                        Accessible.name: i18n("More monitor actions")
                        onClicked: moreMenu.open()
                        QQC2.Menu {
                            id: moreMenu
                            y: moreButton.height
                            QQC2.MenuItem {
                                text: i18n("Run guided setup again")
                                Accessible.name: text
                                icon.name: "tools-wizard"
                                onTriggered: fullRoot.startGuidedSetupAgain()
                            }
                        }
                        PlasmaComponents.ToolTip { text: i18n("More actions") }
                    }
                }
            }
        }

        Loader {
            id: onboardingLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: fullRoot.showGuidedSetup
            visible: fullRoot.showGuidedSetup
            sourceComponent: Onboarding.OnboardingFlow {
                runtime: fullRoot.monitor
                readinessModel: fullRoot.monitor.sourceReadiness
                secretStore: fullRoot.monitor.secretsManager
                configuration: Plasmoid.configuration
                previewState: AppInfo.smokeView
            }
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
                onClicked: fullRoot.resumeGuidedSetup()
            }
        }

        Loader {
            id: destinationLoader
            objectName: "destinationLoader"
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 64
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            active: !fullRoot.showGuidedSetup
            asynchronous: true
            source: fullRoot.sourceDetailVisible ? "views/SourceDetailView.qml"
                  : fullRoot.destination === 0 ? "views/OverviewView.qml"
                  : fullRoot.destination === 1 ? "views/HistoryView.qml"
                  : "views/AnalystView.qml"
            onLoaded: {
                AppInfo.performanceMark("destination_component_loaded");
                item.monitor = fullRoot.monitor;
                if (fullRoot.sourceDetailVisible) {
                    item.sourceId = fullRoot.detailSourceId;
                    item.sessionState = fullRoot.localSessionViewState.sourceDetail;
                } else if (fullRoot.destination === 1) {
                    fullRoot.applyHistoryRoute(item);
                } else if (fullRoot.destination === 2) {
                    item.viewState = fullRoot.insightsViewState;
                    item.sessionState = fullRoot.localSessionViewState.analyst;
                } else if (fullRoot.destination === 0) {
                    item.viewState = fullRoot.overviewViewState;
                    if (fullRoot.returnFocusSourceId !== "") {
                        var sourceId = fullRoot.returnFocusSourceId;
                        fullRoot.returnFocusSourceId = "";
                        Qt.callLater(function() {
                            // qmllint disable missing-property
                            if (destinationLoader.item && typeof destinationLoader.item.restoreSourceFocus === "function") {
                                destinationLoader.item.restoreSourceFocus(sourceId);
                            }
                            // qmllint enable missing-property
                        });
                    }
                }
            }
        }

        Connections {
            target: destinationLoader.item
            enabled: destinationLoader.status === Loader.Ready
            ignoreUnknownSignals: true

            function onSourceRequested(stableId) {
                fullRoot.captureCurrentViewState();
                fullRoot.detailSourceId = stableId;
                fullRoot.returnFocusSourceId = stableId;
                fullRoot.sourceDetailVisible = true;
            }
            function onBackRequested() {
                fullRoot.sourceDetailVisible = false;
                fullRoot.destination = 0;
            }
            function onActionRequested(stableId, actionKey, sourceKind) {
                if (actionKey === "review_quota" || actionKey === "review_runway") {
                    fullRoot.captureCurrentViewState();
                    fullRoot.detailSourceId = stableId;
                    fullRoot.returnFocusSourceId = stableId;
                    fullRoot.sourceDetailVisible = true;
                    return;
                }
                fullRoot.monitor.fixOverviewSource(stableId, actionKey,
                                                   sourceKind);
            }
            function onSettingsRequested(stableId) {
                fullRoot.monitor.fixOverviewSource(
                    stableId, "open_source_settings", "");
            }
            function onHistoryRequested(historyId, metric, rangeDays) {
                fullRoot.captureCurrentViewState();
                fullRoot.historyViewState = ({});
                fullRoot.historySessionsMode = false;
                fullRoot.historySourceId = historyId;
                fullRoot.historyMetric = metric;
                fullRoot.historyRangeDays = rangeDays;
                fullRoot.historyRequestPending = true;
                fullRoot.sourceDetailVisible = false;
                fullRoot.destination = 1;
            }
            function onSessionHistoryRequested(toolId, projectId, rangeDays,
                                               sessionId, pageOffset, modelFilter) {
                fullRoot.routeToSessionHistory(toolId, projectId, rangeDays,
                    sessionId, pageOffset, modelFilter);
            }
            function onSessionHistoryRangeRequested(selection) {
                fullRoot.routeToSessionHistoryRange(selection);
            }
            function onSessionDayReturnRequested() {
                fullRoot.returnToSessionDayOverview();
            }
            function onSessionsModeChanged() {
                if (fullRoot.destination === 1 && !fullRoot.sourceDetailVisible) {
                    // qmllint disable missing-property
                    fullRoot.historySessionsMode = destinationLoader.item.sessionsMode;
                    // qmllint enable missing-property
                }
            }
            function onSessionToolIdChanged() {
                if (fullRoot.destination === 1 && !fullRoot.sourceDetailVisible) {
                    // qmllint disable missing-property
                    fullRoot.historySessionToolId = destinationLoader.item.sessionToolId;
                    // qmllint enable missing-property
                }
            }
            function onSessionProjectIdChanged() {
                if (fullRoot.destination === 1 && !fullRoot.sourceDetailVisible) {
                    // qmllint disable missing-property
                    fullRoot.historySessionProjectId = destinationLoader.item.sessionProjectId;
                    // qmllint enable missing-property
                }
            }
        }
    }

    Component.onCompleted: {
        AppInfo.performanceMark("full_representation_created");
        Qt.callLater(function() {
            AppInfo.performanceMark("first_rendered_frame");
        });
        if (AppInfo.smokeView === "settings"
                || AppInfo.smokeView === "budget-settings") {
            Qt.callLater(function() {
                var configureAction = Plasmoid.internalAction("configure");
                if (configureAction) configureAction.trigger();
            });
        }
    }
}
