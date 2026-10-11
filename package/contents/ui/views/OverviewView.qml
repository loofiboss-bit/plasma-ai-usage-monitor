pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import ".." as Monitor
import "../components" as Components
import "../Utils.js" as Utils

QQC2.ScrollView {
    id: overview

    property var monitor: null
    property string sourceSearchText: ""
    property int sourceFilterIndex: 0
    property int sessionSummaryGeneration: 0
    property var sessionSummaryRequests: ({})
    property var sessionSummaryLatestByTool: ({})
    property var sessionSummaryByTool: ({})
    readonly property var visibleSourceRows: filterSourceRows()
    signal sourceRequested(string stableId)
    Accessible.role: Accessible.Pane
    Accessible.name: i18n("Overview view ready")

    Components.DailyOverviewState {
        id: overviewState
        dailyState: overview.monitor.presentationDailyState
    }

    function restoreSourceFocus(stableId) {
        for (var i = 0; i < sourceRepeater.count; ++i) {
            // qmllint disable missing-property
            var card = sourceRepeater.itemAt(i);
            if (card && card.row.stableId === stableId) {
                card.forceActiveFocus(Qt.BacktabFocusReason);
                // qmllint enable missing-property
                return true;
            }
            // qmllint enable missing-property
        }
        return false;
    }

    function filterSourceRows() {
        return Utils.filterOverviewSourceRows(overviewState.sourceRows,
                                              sourceSearchText,
                                              sourceFilterIndex === 1);
    }

    function refreshSessionSummaries() {
        if (!monitor || !monitor.localSessions) return;
        var model = monitor.localSessions;
        var now = new Date();
        var start = new Date(now.getTime());
        start.setHours(0, 0, 0, 0);
        var from = start.toISOString();
        var to = now.toISOString();
        var tools = [
            { id: "codex-cli", enabled: !!model.codexEnabled },
            { id: "claude-code", enabled: !!model.claudeCodeEnabled }
        ];
        for (var i = 0; i < tools.length; ++i) {
            if (!tools[i].enabled) {
                var disabledState = Object.assign({}, sessionSummaryByTool);
                delete disabledState[tools[i].id];
                sessionSummaryByTool = disabledState;
                var latestDisabled = Object.assign({}, sessionSummaryLatestByTool);
                delete latestDisabled[tools[i].id];
                sessionSummaryLatestByTool = latestDisabled;
                continue;
            }
            var requestId = "overview-session-summary-"
                + (++sessionSummaryGeneration);
            var requests = Object.assign({}, sessionSummaryRequests);
            requests[requestId] = tools[i].id;
            sessionSummaryRequests = requests;
            var latest = Object.assign({}, sessionSummaryLatestByTool);
            latest[tools[i].id] = requestId;
            sessionSummaryLatestByTool = latest;
            model.requestSummary(requestId, tools[i].id, from, to, "");
        }
    }

    function sessionActivityText(stableId) {
        if (stableId !== "codex-cli" && stableId !== "claude-code") return "";
        var model = monitor ? monitor.localSessions : null;
        if (!model) return "";
        var enabled = stableId === "codex-cli"
            ? !!model.codexEnabled : !!model.claudeCodeEnabled;
        if (!enabled) return "";
        var summary = sessionSummaryByTool[stableId] || ({});
        var count = Math.max(0, Number(summary.sessionCount || 0));
        var lastActivity = Date.parse(summary.lastActivityAtUtc || "");
        var activityText = Number.isFinite(lastActivity)
            ? new Date(lastActivity).toLocaleString(Qt.locale())
            : i18n("not observed");
        var partial = String(summary.coverage || "").indexOf("partial") === 0
            || summary.importStatus === "error";
        return i18n("%1 sessions today · last activity %2", count, activityText)
            + (partial
                ? i18n(" · partial import") : "");
    }

    Connections {
        target: overview.monitor ? overview.monitor.localSessions : null
        enabled: overview.monitor !== null && overview.monitor.localSessions !== null
        ignoreUnknownSignals: true
        function onSummaryReady(requestId, summary) {
            var toolId = overview.sessionSummaryRequests[requestId];
            if (!toolId) return;
            var requests = Object.assign({}, overview.sessionSummaryRequests);
            delete requests[requestId];
            overview.sessionSummaryRequests = requests;
            if (overview.sessionSummaryLatestByTool[toolId] !== requestId) return;
            var next = Object.assign({}, overview.sessionSummaryByTool);
            next[toolId] = summary || {};
            overview.sessionSummaryByTool = next;
        }
        function onImportFinished() { overview.refreshSessionSummaries(); }
        function onEnabledChanged() { overview.refreshSessionSummaries(); }
    }

    Timer {
        interval: 60000
        repeat: true
        running: overview.visible
        onTriggered: overview.refreshSessionSummaries()
    }

    ColumnLayout {
        width: overview.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        Components.DailyFocus {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            presentation: overviewState
            guardrails: overview.monitor.guardrails
            forecastsEnabled: Plasmoid.configuration.forecastUiEnabled
            onActionRequested: function(stableId, actionKey, sourceKind) {
                if (actionKey === "review_runway") {
                    overview.sourceRequested(stableId);
                    return;
                }
                overview.monitor.fixOverviewSource(stableId, actionKey,
                                                    sourceKind);
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overview.monitor.modelMigrationNotice !== ""
            text: overview.monitor.modelMigrationNotice
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing

            PlasmaExtras.Heading {
                level: 4
                text: i18n("Sources")
                Layout.fillWidth: true
            }
            PlasmaComponents.Label {
                text: overview.visibleSourceRows.length === overviewState.sourceRows.length
                    ? overviewState.sourceRows.length
                    : i18n("%1 shown · %2 total", overview.visibleSourceRows.length,
                           overviewState.sourceRows.length)
                color: Kirigami.Theme.disabledTextColor
                Accessible.name: i18np("%1 source", "%1 sources",
                                             overviewState.sourceRows.length)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.sourceRows.length === 0
            text: i18n("Enable a source or run guided setup to start monitoring.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: overviewState.sourceRows.length > 0

            Kirigami.SearchField {
                Layout.fillWidth: true
                placeholderText: i18n("Search sources")
                Accessible.name: i18n("Search monitoring sources")
                text: overview.sourceSearchText
                onTextChanged: overview.sourceSearchText = text
            }

            QQC2.ComboBox {
                Layout.minimumWidth: Kirigami.Units.gridUnit * 8
                model: [i18n("All"), i18n("Needs attention")]
                currentIndex: overview.sourceFilterIndex
                Accessible.name: i18n("Filter sources")
                onActivated: function(index) { overview.sourceFilterIndex = index; }
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.sourceRows.length > 0
                && overview.visibleSourceRows.length === 0
            text: overview.sourceFilterIndex === 1
                ? i18n("No sources currently need attention.")
                : i18n("No source matches your search.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        Repeater {
            id: sourceRepeater
            model: overview.visibleSourceRows

            Monitor.DailySourceCard {
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                row: modelData
                sessionActivityText: overview.sessionActivityText(modelData.stableId)
                onSourceRequested: function(stableId) {
                    overview.sourceRequested(stableId);
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }

    Component.onCompleted: Qt.callLater(refreshSessionSummaries)
}
