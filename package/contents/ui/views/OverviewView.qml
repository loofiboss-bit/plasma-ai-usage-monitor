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
    property bool costsExpanded: false
    property var viewState: ({})
    readonly property var visibleSourceRows: filterSourceRows()
    readonly property var visibleSourceGroups:
        Utils.groupOverviewSourceRows(visibleSourceRows)
    signal sourceRequested(string stableId)
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    Accessible.role: Accessible.Pane
    Accessible.name: i18n("Overview view ready")

    Components.DailyOverviewState {
        id: overviewState
        dailyState: overview.monitor.presentationDailyState
    }

    function restoreSourceFocus(stableId) {
        for (var i = 0; i < sourceGroupRepeater.count; ++i) {
            // qmllint disable missing-property
            var group = sourceGroupRepeater.itemAt(i);
            if (group && group.restoreSourceFocus(stableId)) return true;
            // qmllint enable missing-property
        }
        return false;
    }

    function captureViewState() {
        var contentY = 0;
        // Controls exposes a Flickable here at runtime; qmltypes declare QQuickItem.
        // qmllint disable missing-property
        if (overview.contentItem && overview.contentItem.contentY !== undefined)
            contentY = overview.contentItem.contentY;
        // qmllint enable missing-property
        return {
            searchText: sourceSearchText,
            filterIndex: sourceFilterIndex,
            costsExpanded: costsExpanded,
            contentY: contentY
        };
    }

    function applyViewState() {
        var saved = viewState || ({});
        sourceSearchText = saved.searchText || "";
        sourceFilterIndex = saved.filterIndex === 1 ? 1 : 0;
        costsExpanded = !!saved.costsExpanded;
        Qt.callLater(function() {
            // The QML type for ScrollView contentItem is generic; at runtime it is a Flickable.
            // qmllint disable missing-property
            if (!overview.contentItem || overview.contentItem.contentY === undefined)
                return;
            var maxY = Math.max(0, overview.contentItem.contentHeight
                - overview.contentItem.height);
            overview.contentItem.contentY = Math.max(0,
                Math.min(Number(saved.contentY || 0), maxY));
            // qmllint enable missing-property
        });
    }

    onViewStateChanged: applyViewState()

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
        if (!Object.prototype.hasOwnProperty.call(sessionSummaryByTool, stableId))
            return i18n("Loading session summary…");
        var summary = sessionSummaryByTool[stableId] || ({});
        var count = Math.max(0, Number(summary.sessionCount || 0));
        var lastActivity = Date.parse(summary.lastActivityAtUtc || "");
        var activityText = Number.isFinite(lastActivity)
            ? Utils.formatWhen(lastActivity, "relative")
            : i18n("not observed");
        var partial = String(summary.coverage || "").indexOf("partial") === 0
            || summary.importStatus === "error";
        return i18np("%1 session today", "%1 sessions today", count)
            + i18n(" · last activity %1", activityText)
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
                overview.actionRequested(stableId, actionKey, sourceKind);
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: costSummary.spendRows.length > 0

            PlasmaComponents.Button {
                objectName: "overviewMoreCostsToggle"
                text: i18n("More costs")
                icon.name: overview.costsExpanded ? "arrow-down" : "arrow-right"
                checkable: true
                checked: overview.costsExpanded
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: overview.costsExpanded = checked
            }
            Item { Layout.fillWidth: true }
        }

        Monitor.CostSummaryCard {
            id: costSummary
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            summary: overviewState.summary
            visible: overview.costsExpanded && spendRows.length > 0
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
                text: overview.sourceSearchText.trim() === ""
                        && overview.sourceFilterIndex === 0
                    ? i18np("%1 source", "%1 sources", overviewState.sourceRows.length)
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

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            columns: overview.width >= Kirigami.Units.gridUnit * 30 ? 2 : 1
            visible: overviewState.sourceRows.length > 0

            Kirigami.SearchField {
                Layout.fillWidth: true
                placeholderText: i18n("Search sources")
                Accessible.name: i18n("Search monitoring sources")
                text: overview.sourceSearchText
                onTextChanged: overview.sourceSearchText = text
            }

            QQC2.ComboBox {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
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
            id: sourceGroupRepeater
            model: overview.visibleSourceGroups

            delegate: ColumnLayout {
                id: sourceGroup
                required property var modelData
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaExtras.Heading {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    level: 4
                    text: i18n("%1 · %2", sourceGroup.modelData.label,
                               sourceGroup.modelData.rows.length)
                }

                Repeater {
                    id: sourceRowsRepeater
                    model: sourceGroup.modelData.rows

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

                function restoreSourceFocus(stableId) {
                    for (var i = 0; i < sourceRowsRepeater.count; ++i) {
                        // qmllint disable missing-property
                        var card = sourceRowsRepeater.itemAt(i);
                        if (card && card.row.stableId === stableId) {
                            card.forceActiveFocus(Qt.BacktabFocusReason);
                            // qmllint enable missing-property
                            return true;
                        }
                        // qmllint enable missing-property
                    }
                    return false;
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }

    Component.onCompleted: {
        applyViewState();
        Qt.callLater(refreshSessionSummaries);
    }
}
