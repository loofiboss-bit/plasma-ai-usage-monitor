pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0

ColumnLayout {
    id: panel

    required property var sessionModel
    property string fixedToolId: ""
    property string initialToolId: "codex-cli"
    property string initialProjectId: ""
    property int initialRangeDays: 7
    property var sessionState: null
    property string selectedToolId: "codex-cli"
    property string selectedProjectId: ""
    property int selectedRangeIndex: 1
    property int pageOffset: 0
    property string selectedSessionId: ""
    property var projectRows: []
    property var sessionRows: []
    property var dailyRows: []
    property var summary: ({})
    property bool loading: false
    property string statusText: ""
    property string summaryRequestId: ""
    property string sessionsRequestId: ""
    property string dailyRequestId: ""
    property string projectsRequestId: ""
    property string exportRequestId: ""
    property string pendingExportFormat: "json"
    property int requestGeneration: 0
    readonly property string requestPrefix: Date.now().toString(36)
        + "-" + Math.random().toString(36).slice(2)
    readonly property bool mediaMode: AppInfo.demoMode
        && ["media-coding-sessions", "media-history-sessions",
            "media-analyst-sessions"].indexOf(AppInfo.smokeView) >= 0
    readonly property var activeSessionModel: mediaMode
        ? mediaSessionModel : sessionModel

    signal sessionHistoryRequested(string toolId, string projectId, int rangeDays,
                                   string sessionId, int pageOffset)
    signal filtersChanged(string toolId, string projectId, int rangeDays)
    Accessible.role: Accessible.Pane
    Accessible.name: i18n("Local coding session history")

    MediaLocalSessionModel {
        id: mediaSessionModel
        toolId: panel.activeToolId
    }

    readonly property string activeToolId: fixedToolId.length > 0
        ? fixedToolId : selectedToolId
    readonly property int rangeDays: [1, 7, 30][selectedRangeIndex] || 7

    function localRange() {
        var now = new Date();
        var from = new Date(now.getTime());
        from.setHours(0, 0, 0, 0);
        from.setDate(from.getDate() - rangeDays + 1);
        return { from: from.toISOString(), to: now.toISOString() };
    }

    function persistState() {
        if (!sessionState) return;
        sessionState.toolId = selectedToolId;
        sessionState.projectId = selectedProjectId;
        sessionState.rangeIndex = selectedRangeIndex;
        sessionState.pageOffset = pageOffset;
        sessionState.sessionId = selectedSessionId;
    }

    function restoreState() {
        if (!sessionState) return;
        selectedToolId = String(sessionState.toolId || initialToolId || selectedToolId);
        selectedProjectId = String(sessionState.projectId || initialProjectId || "");
        selectedRangeIndex = Math.max(0, Math.min(2,
            Number(sessionState.rangeIndex === undefined
                ? ([1, 7, 30].indexOf(Number(initialRangeDays)) < 0 ? 1
                    : [1, 7, 30].indexOf(Number(initialRangeDays)))
                : sessionState.rangeIndex)));
        pageOffset = Math.max(0, Number(sessionState.pageOffset || 0));
        selectedSessionId = String(sessionState.sessionId || "");
    }

    function toolOptions() {
        if (fixedToolId === "codex-cli") return [{ label: i18n("Codex"), value: "codex-cli" }];
        if (fixedToolId === "claude-code") return [{ label: i18n("Claude Code"), value: "claude-code" }];
        return [
            { label: i18n("Codex"), value: "codex-cli" },
            { label: i18n("Claude Code"), value: "claude-code" }
        ];
    }

    function selectedToolEnabled() {
        if (!activeSessionModel) return false;
        return activeToolId === "codex-cli"
            ? !!activeSessionModel.codexEnabled : !!activeSessionModel.claudeCodeEnabled;
    }

    function displayedProjects() {
        var rows = [{ label: i18n("All projects"), projectId: "" }];
        for (var i = 0; i < projectRows.length; ++i) {
            var row = projectRows[i] || {};
            if (row.toolId !== activeToolId) continue;
            var alias = String(row.alias || "").trim();
            rows.push({
                label: alias || i18n("Unnamed project %1", rows.length),
                projectId: String(row.projectId || "")
            });
        }
        return rows;
    }

    function projectIndex() {
        var rows = displayedProjects();
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].projectId === selectedProjectId) return i;
        }
        return 0;
    }

    function dateText(value) {
        var time = Date.parse(value || "");
        return Number.isFinite(time) ? new Date(time).toLocaleString(Qt.locale())
                                     : i18n("Unavailable");
    }

    function tokenText(value) {
        if (value === undefined || value === null || !Number.isFinite(Number(value)))
            return i18n("Unavailable");
        return Math.round(Number(value)).toLocaleString(Qt.locale());
    }

    function coverageText(value) {
        if (value === "reported") return i18n("Reported usage data");
        if (value === "complete") return i18n("Complete coverage");
        if (value === "partial") return i18n("Partial coverage");
        if (value === "partial-counter-reset") return i18n("Partial after a counter reset");
        if (value === "conflict") return i18n("Conflicting records");
        if (value === "unverified_lineage") return i18n("Unverified fork or agent history");
        return value ? String(value) : i18n("Coverage unavailable");
    }

    function projectLabel(projectId) {
        for (var i = 0; i < projectRows.length; ++i) {
            if (projectRows[i].projectId === projectId) {
                var alias = String(projectRows[i].alias || "").trim();
                return alias || i18n("Unnamed project");
            }
        }
        return i18n("Project unavailable");
    }

    function modelNames(rows) {
        var names = [];
        rows = rows || [];
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].modelId && names.indexOf(rows[i].modelId) < 0)
                names.push(rows[i].modelId);
        }
        return names;
    }

    function loadProjects() {
        if (!activeSessionModel || typeof activeSessionModel.requestProjects !== "function") return;
        if (!selectedToolEnabled()) {
            projectRows = [];
            return;
        }
        projectsRequestId = "session-projects-" + requestPrefix
            + "-" + (++requestGeneration);
        activeSessionModel.requestProjects(projectsRequestId);
    }

    function refresh() {
        if (!sessionModel || !selectedToolEnabled()) {
            loading = false;
            sessionRows = [];
            summary = ({});
            return;
        }
        var range = localRange();
        var generation = ++requestGeneration;
        summaryRequestId = "session-summary-" + requestPrefix + "-" + generation;
        sessionsRequestId = "session-list-" + requestPrefix + "-" + generation;
        loading = true;
        statusText = "";
        activeSessionModel.requestSummary(summaryRequestId, activeToolId,
            range.from, range.to, selectedProjectId);
        activeSessionModel.requestSessions(sessionsRequestId, activeToolId,
            range.from, range.to, selectedProjectId, pageOffset, 100);
        dailyRequestId = "session-daily-" + requestPrefix + "-" + generation;
        activeSessionModel.requestDaily(dailyRequestId, activeToolId,
            range.from, range.to, selectedProjectId, "");
    }

    function openExport(format) {
        pendingExportFormat = format;
        exportDialog.nameFilters = format === "csv"
            ? [i18n("CSV files (*.csv)")] : [i18n("JSON files (*.json)")];
        exportDialog.currentFile = "coding-sessions." + format;
        exportDialog.open();
    }

    function beginExport(path) {
        var range = localRange();
        exportRequestId = "session-export-" + requestPrefix
            + "-" + (++requestGeneration);
        statusText = i18n("Writing redacted export…");
        activeSessionModel.requestExport(exportRequestId, activeToolId,
            range.from, range.to, selectedProjectId, pendingExportFormat, path);
    }

    function localPath(url) {
        var value = String(url || "");
        if (value.indexOf("file:") === 0) {
            value = value.replace(/^file:\/\/\/?/, "");
            try { return decodeURIComponent(value); }
            catch (error) { return ""; }
        }
        return value;
    }

    function acceptSessionRows(requestId, rows) {
        if (requestId !== sessionsRequestId) return;
        sessionRows = rows || [];
        loading = false;
        var found = false;
        var selectedIndex = -1;
        for (var i = 0; i < sessionRows.length; ++i) {
            if (sessionRows[i].sessionId === selectedSessionId) {
                found = true;
                selectedIndex = i;
            }
        }
        if (!found) selectedSessionId = "";
        persistState();
        if (found) {
            Qt.callLater(function() {
                var delegate = sessionList.itemAtIndex(selectedIndex);
                if (delegate) delegate.forceActiveFocus(Qt.TabFocusReason);
            });
        }
    }

    Connections {
        target: panel.activeSessionModel
        enabled: panel.activeSessionModel !== null
        ignoreUnknownSignals: true

        function onImportFinished() {
            if (panel.visible) {
                panel.loadProjects();
                panel.refresh();
            }
        }

        function onProjectsReady(requestId, rows) {
            if (requestId !== panel.projectsRequestId) return;
            panel.projectRows = rows || [];
            if (panel.selectedProjectId !== ""
                    && panel.projectIndex() === 0) {
                panel.selectedProjectId = "";
                panel.persistState();
                panel.refresh();
            }
        }

        function onSummaryReady(requestId, payload) {
            if (requestId !== panel.summaryRequestId) return;
            panel.summary = payload || {};
        }

        function onSessionsReady(requestId, rows) {
            panel.acceptSessionRows(requestId, rows);
        }

        function onDailyReady(requestId, rows) {
            if (requestId !== panel.dailyRequestId) return;
            panel.dailyRows = rows || [];
        }

        function onExportReady(requestId, errorKey) {
            if (requestId !== panel.exportRequestId) return;
            panel.statusText = errorKey === ""
                ? i18n("Redacted session export saved.")
                : i18n("Session export failed: %1", errorKey);
        }

        function onEnabledChanged() {
            panel.refresh();
            panel.loadProjects();
        }
    }

    Kirigami.Heading {
        Layout.fillWidth: true
        level: 4
        text: i18n("Coding sessions")
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        visible: !panel.selectedToolEnabled()
        text: i18n("Enable %1 session history in Settings → History and press Apply to import local sessions.",
                   panel.activeToolId === "codex-cli" ? i18n("Codex") : i18n("Claude Code"))
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled() && panel.dailyRows.length > 0
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 5
            text: i18n("Daily usage")
        }
        ListView {
            id: dailyList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(
                panel.dailyRows.length * Kirigami.Units.gridUnit * 3,
                Kirigami.Units.gridUnit * 7)
            Layout.minimumHeight: Kirigami.Units.gridUnit * 3
            clip: true
            model: panel.dailyRows
            spacing: Kirigami.Units.smallSpacing
            Accessible.name: i18n("Daily session token usage")

            delegate: ColumnLayout {
                id: dailyRow
                required property var modelData
                width: dailyList.width
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: dailyRow.modelData.localDate || i18n("Date unavailable")
                        font.bold: true
                    }
                    PlasmaComponents.Label {
                        text: i18n("%1 sessions",
                            panel.tokenText(dailyRow.modelData.sessionCount))
                    }
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Input %1 · output %2 · cache read %3 · cache write %4",
                        panel.tokenText(dailyRow.modelData.inputTokens),
                        panel.tokenText(dailyRow.modelData.outputTokens),
                        panel.tokenText(dailyRow.modelData.cacheReadTokens),
                        panel.tokenText(dailyRow.modelData.cacheWriteTokens))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: width < Kirigami.Units.gridUnit * 28 ? 2 : 4
        visible: panel.selectedToolEnabled()

        QQC2.ComboBox {
            Layout.fillWidth: true
            visible: panel.fixedToolId === ""
            model: panel.toolOptions()
            textRole: "label"
            valueRole: "value"
            currentIndex: panel.activeToolId === "claude-code" ? 1 : 0
            Accessible.name: i18n("Session tool")
            onActivated: {
                panel.selectedToolId = currentValue;
                panel.selectedProjectId = "";
                panel.pageOffset = 0;
                panel.persistState();
                panel.refresh();
                panel.filtersChanged(panel.activeToolId, panel.selectedProjectId,
                                     panel.rangeDays);
            }
        }

        QQC2.ComboBox {
            Layout.fillWidth: true
            model: [i18n("Today"), i18n("7 days"), i18n("30 days")]
            currentIndex: panel.selectedRangeIndex
            Accessible.name: i18n("Session period")
            onActivated: {
                panel.selectedRangeIndex = currentIndex;
                panel.pageOffset = 0;
                panel.persistState();
                panel.refresh();
                panel.filtersChanged(panel.activeToolId, panel.selectedProjectId,
                                     panel.rangeDays);
            }
        }

        QQC2.ComboBox {
            Layout.fillWidth: true
            model: panel.displayedProjects()
            textRole: "label"
            valueRole: "projectId"
            currentIndex: panel.projectIndex()
            Accessible.name: i18n("Session project")
            onActivated: {
                panel.selectedProjectId = currentValue || "";
                panel.pageOffset = 0;
                panel.persistState();
                panel.refresh();
                panel.filtersChanged(panel.activeToolId, panel.selectedProjectId,
                                     panel.rangeDays);
            }
        }

        PlasmaComponents.Button {
            Layout.fillWidth: true
            text: i18n("Export")
            icon.name: "document-export"
            activeFocusOnTab: true
            enabled: !panel.loading && panel.selectedToolEnabled()
            Accessible.name: i18n("Export filtered session history")
            onClicked: exportMenu.open()

            QQC2.Menu {
                id: exportMenu
                y: parent.height
                QQC2.MenuItem {
                    text: i18n("Export JSON")
                    onTriggered: panel.openExport("json")
                }
                QQC2.MenuItem {
                    text: i18n("Export CSV")
                    onTriggered: panel.openExport("csv")
                }
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: width < Kirigami.Units.gridUnit * 28 ? 2 : 4
        visible: panel.selectedToolEnabled()
        PlasmaComponents.Label { text: i18n("Sessions: %1", panel.tokenText(panel.summary.sessionCount)) }
        PlasmaComponents.Label { text: i18n("Events: %1", panel.tokenText(panel.summary.eventCount)) }
        PlasmaComponents.Label { text: i18n("Input: %1", panel.tokenText(panel.summary.inputTokens)) }
        PlasmaComponents.Label { text: i18n("Output: %1", panel.tokenText(panel.summary.outputTokens)) }
        PlasmaComponents.Label { text: i18n("Cache read: %1", panel.tokenText(panel.summary.cacheReadTokens)) }
        PlasmaComponents.Label { text: i18n("Cache write: %1", panel.tokenText(panel.summary.cacheWriteTokens)) }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.columnSpan: parent.columns
            visible: !!panel.summary.coverage
            text: panel.coverageText(panel.summary.coverage)
            color: panel.summary.coverage === "complete"
                ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
            Accessible.name: text
        }
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        visible: panel.statusText !== ""
        text: panel.statusText
        wrapMode: Text.WordWrap
        color: panel.statusText.indexOf("failed") >= 0
            ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.neutralTextColor
        Accessible.name: text
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
            && (panel.summary.modelBreakdown || []).length > 0
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 5
            text: i18n("By model")
        }
        ListView {
            id: modelBreakdownList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(
                (panel.summary.modelBreakdown || []).length
                    * Kirigami.Units.gridUnit * 2,
                Kirigami.Units.gridUnit * 5)
            clip: true
            model: panel.summary.modelBreakdown || []
            spacing: Kirigami.Units.smallSpacing
            Accessible.name: i18n("Token usage by model")

            delegate: RowLayout {
                id: modelBreakdownRow
                required property var modelData
                width: modelBreakdownList.width
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: modelBreakdownRow.modelData.modelId || i18n("Model unavailable")
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    text: i18n("%1 events · input %2 · output %3",
                        panel.tokenText(modelBreakdownRow.modelData.eventCount),
                        panel.tokenText(modelBreakdownRow.modelData.inputTokens),
                        panel.tokenText(modelBreakdownRow.modelData.outputTokens))
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
            && !panel.selectedProjectId
            && (panel.summary.projectBreakdown || []).length > 0
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 5
            text: i18n("By project")
        }
        ListView {
            id: projectBreakdownList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(
                (panel.summary.projectBreakdown || []).length
                    * Kirigami.Units.gridUnit * 2,
                Kirigami.Units.gridUnit * 5)
            clip: true
            model: panel.summary.projectBreakdown || []
            spacing: Kirigami.Units.smallSpacing
            Accessible.name: i18n("Token usage by project")

            delegate: RowLayout {
                id: projectBreakdownRow
                required property var modelData
                width: projectBreakdownList.width
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: String(projectBreakdownRow.modelData.alias || "").trim()
                        || i18n("Unnamed project")
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    text: i18n("%1 sessions · input %2 · output %3",
                        panel.tokenText(projectBreakdownRow.modelData.sessionCount),
                        panel.tokenText(projectBreakdownRow.modelData.inputTokens),
                        panel.tokenText(projectBreakdownRow.modelData.outputTokens))
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }

    PlasmaComponents.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: visible
        visible: panel.loading
    }

    ListView {
        id: sessionList
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Kirigami.Units.gridUnit * 10
        clip: true
        visible: panel.selectedToolEnabled()
        model: panel.sessionRows
        spacing: Kirigami.Units.smallSpacing
        Accessible.name: i18n("Coding sessions")

        delegate: QQC2.ItemDelegate {
            id: sessionDelegate
            required property var modelData
            width: sessionList.width
            activeFocusOnTab: true
            Accessible.name: i18n("Session ending %1, %2 events",
                panel.dateText(modelData.lastSeenAtUtc), panel.tokenText(modelData.eventCount))
            Accessible.description: panel.coverageText(modelData.coverage)
            onClicked: {
                panel.selectedSessionId = panel.selectedSessionId === modelData.sessionId
                    ? "" : modelData.sessionId;
                panel.persistState();
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("%1 · %2 events",
                            panel.dateText(sessionDelegate.modelData.lastSeenAtUtc),
                            panel.tokenText(sessionDelegate.modelData.eventCount))
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    PlasmaComponents.Label {
                        text: panel.coverageText(sessionDelegate.modelData.coverage)
                        color: sessionDelegate.modelData.coverage === "complete"
                            ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                    }
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Input %1 · Output %2 · Cache read %3 · Cache write %4",
                        panel.tokenText(sessionDelegate.modelData.inputTokens),
                        panel.tokenText(sessionDelegate.modelData.outputTokens),
                        panel.tokenText(sessionDelegate.modelData.cacheReadTokens),
                        panel.tokenText(sessionDelegate.modelData.cacheWriteTokens))
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: panel.projectLabel(sessionDelegate.modelData.projectId)
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: panel.selectedSessionId === sessionDelegate.modelData.sessionId
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("Started: %1 · Last activity: %2",
                            panel.dateText(sessionDelegate.modelData.startedAtUtc),
                            panel.dateText(sessionDelegate.modelData.lastSeenAtUtc))
                        wrapMode: Text.WordWrap
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: panel.modelNames(
                            sessionDelegate.modelData.modelBreakdown).length > 0
                        text: i18n("Models: %1", panel.modelNames(
                            sessionDelegate.modelData.modelBreakdown).join(", "))
                        wrapMode: Text.WordWrap
                    }
                    PlasmaComponents.Button {
                        text: i18n("Open this filter in History")
                        visible: panel.fixedToolId !== ""
                        activeFocusOnTab: true
                        Accessible.name: text
                        onClicked: panel.sessionHistoryRequested(
                            panel.activeToolId, panel.selectedProjectId, panel.rangeDays,
                            panel.selectedSessionId, panel.pageOffset)
                    }
                }
            }
        }

        PlasmaComponents.Label {
            anchors.centerIn: parent
            visible: !panel.loading && panel.sessionRows.length === 0
            text: i18n("No sessions matched this period and project filter.")
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
        PlasmaComponents.Button {
            text: i18n("Previous")
            enabled: panel.pageOffset > 0 && !panel.loading
            onClicked: {
                panel.pageOffset = Math.max(0, panel.pageOffset - 100);
                panel.persistState();
                panel.refresh();
            }
        }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: panel.sessionRows.length > 0
                ? i18n("Rows %1–%2", panel.pageOffset + 1,
                       panel.pageOffset + panel.sessionRows.length)
                : i18n("No rows")
        }
        PlasmaComponents.Button {
            text: i18n("Next")
            enabled: panel.sessionRows.length === 100 && !panel.loading
            onClicked: {
                panel.pageOffset += 100;
                panel.persistState();
                panel.refresh();
            }
        }
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export coding sessions")
        fileMode: Dialogs.FileDialog.SaveFile
        onAccepted: panel.beginExport(panel.localPath(selectedFile))
    }

    onVisibleChanged: {
        if (visible) {
            loadProjects();
            refresh();
        }
    }

    onSelectedToolIdChanged: {
        if (fixedToolId === "") {
            loadProjects();
            refresh();
        }
    }
    onSelectedProjectIdChanged: persistState()
    onSelectedRangeIndexChanged: persistState()
    onSessionStateChanged: {
        restoreState();
        if (fixedToolId !== "") selectedToolId = fixedToolId;
        refresh();
    }
    onFixedToolIdChanged: {
        selectedToolId = fixedToolId || selectedToolId;
        loadProjects();
        refresh();
    }

    Component.onCompleted: {
        restoreState();
        if (fixedToolId !== "") selectedToolId = fixedToolId;
        loadProjects();
        refresh();
    }
}
