pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "../Utils.js" as Utils

ColumnLayout {
    id: panel
    spacing: Kirigami.Units.largeSpacing

    component SessionSection: QQC2.Frame {
        padding: Kirigami.Units.largeSpacing
        background: Rectangle {
            color: Kirigami.Theme.alternateBackgroundColor
            radius: Kirigami.Units.smallSpacing
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.18)
        }
    }

    component MetricLabel: ColumnLayout {
        id: metricLabel
        required property string title
        required property var value
        property bool count: false
        property bool prominent: false
        Layout.fillWidth: true
        spacing: 0
        QQC2.Label {
            visible: metricLabel.prominent
            Layout.fillWidth: true
            text: metricLabel.title
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            activeFocusOnTab: true
            text: metricLabel.prominent
                ? (metricLabel.count ? panel.countText(metricLabel.value) : panel.tokenText(metricLabel.value))
                : i18n("%1: %2", metricLabel.title,
                       metricLabel.count ? panel.countText(metricLabel.value) : panel.tokenText(metricLabel.value))
            font.bold: metricLabel.prominent
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * (metricLabel.prominent ? 1.4 : 1)
            Accessible.name: i18n("%1: %2", metricLabel.title, panel.countText(metricLabel.value))
            QQC2.ToolTip.visible: metricHover.hovered || activeFocus
            QQC2.ToolTip.text: Accessible.name
            HoverHandler { id: metricHover }
        }
    }

    // Presentation only; shared filters and requests remain unchanged.
    property string presentationMode: "summary"
    property bool technicalDetailsExpanded: false
    property int breakdownIndex: 0
    property int historyDisplayIndex: 0
    readonly property bool showDaily: presentationMode === "analysis"
        ? breakdownIndex === 0 : presentationMode === "history" && historyDisplayIndex === 1

    required property var sessionModel
    property string fixedToolId: ""
    property string initialToolId: "codex-cli"
    property string initialProjectId: ""
    property int initialRangeDays: 7
    property var sessionState: null
    property bool restoringState: false
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
        if (!sessionState || restoringState) return;
        sessionState.toolId = selectedToolId;
        sessionState.projectId = selectedProjectId;
        sessionState.rangeIndex = selectedRangeIndex;
        sessionState.pageOffset = pageOffset;
        sessionState.sessionId = selectedSessionId;
    }

    function restoreState() {
        if (!sessionState) return;
        restoringState = true;
        selectedToolId = String(sessionState.toolId || initialToolId || selectedToolId);
        selectedProjectId = String(sessionState.projectId || initialProjectId || "");
        selectedRangeIndex = Math.max(0, Math.min(2,
            Number(sessionState.rangeIndex === undefined
                ? ([1, 7, 30].indexOf(Number(initialRangeDays)) < 0 ? 1
                    : [1, 7, 30].indexOf(Number(initialRangeDays)))
                : sessionState.rangeIndex)));
        pageOffset = Math.max(0, Number(sessionState.pageOffset || 0));
        selectedSessionId = String(sessionState.sessionId || "");
        restoringState = false;
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
        return Number.isFinite(time) ? new Date(time).toLocaleString(Qt.locale(), Locale.ShortFormat)
                                     : i18n("Unavailable");
    }

    function fullDateText(value) {
        var time = Date.parse(value || "");
        return Number.isFinite(time) ? new Date(time).toLocaleString(Qt.locale())
                                     : i18n("Unavailable");
    }

    function dayText(value) {
        // Daily rows already contain a local calendar date, not a UTC timestamp.
        var parts = /^(\d{4})-(\d{2})-(\d{2})$/.exec(String(value || ""));
        if (!parts) return i18n("Date unavailable");
        var year = Number(parts[1]);
        var month = Number(parts[2]);
        var day = Number(parts[3]);
        var date = new Date(year, month - 1, day);
        if (year < 100) date.setFullYear(year);
        if (date.getFullYear() !== year || date.getMonth() !== month - 1
                || date.getDate() !== day) return i18n("Date unavailable");
        return date.toLocaleDateString(Qt.locale(), Locale.ShortFormat);
    }

    function tokenText(value) {
        return Utils.formatNumber(value) || i18n("Unavailable");
    }

    function countText(value) {
        return Utils.formatCount(value) || i18n("Unavailable");
    }

    function openSessionHistory() {
        sessionHistoryRequested(activeToolId, selectedProjectId, rangeDays,
                                selectedSessionId, pageOffset);
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
        if (url && typeof url.toLocalFile === "function")
            return url.toLocalFile();

        var value = String(url || "");
        if (value.indexOf("file:") === 0) {
            value = value.replace(/^file:\/\//, "");
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

    SessionSection {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
        contentItem: GridLayout {
            columns: width < Kirigami.Units.gridUnit * 40 ? 1 : 2

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
    }

    SessionSection {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
        contentItem: ColumnLayout {
            GridLayout {
                Layout.fillWidth: true
                columns: width < Kirigami.Units.gridUnit * 40 ? 1 : 2
                MetricLabel { title: i18n("Sessions"); value: panel.summary.sessionCount; count: true; prominent: true }
                MetricLabel { title: i18n("Input tokens"); value: panel.summary.inputTokens; prominent: true }
                MetricLabel { title: i18n("Output tokens"); value: panel.summary.outputTokens; prominent: true }
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: panel.coverageText(panel.summary.coverage)
                wrapMode: Text.WordWrap
                color: panel.summary.coverage === "complete"
                    ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                Accessible.name: text
            }
            QQC2.Button {
                objectName: "sessionTechnicalDetailsButton"
                text: i18n("Technical details")
                checkable: true
                checked: panel.technicalDetailsExpanded
                onToggled: panel.technicalDetailsExpanded = checked
                Accessible.description: checked ? i18n("Expanded") : i18n("Collapsed")
            }
            GridLayout {
                objectName: "sessionTechnicalDetails"
                Layout.fillWidth: true
                visible: panel.technicalDetailsExpanded
                columns: width < Kirigami.Units.gridUnit * 40 ? 1 : 2
                MetricLabel { title: i18n("Sessions"); value: panel.summary.sessionCount; count: true }
                MetricLabel { title: i18n("Events"); value: panel.summary.eventCount; count: true }
                MetricLabel { title: i18n("Input tokens"); value: panel.summary.inputTokens; count: true }
                MetricLabel { title: i18n("Output tokens"); value: panel.summary.outputTokens; count: true }
                MetricLabel { title: i18n("Cache read tokens"); value: panel.summary.cacheReadTokens; count: true }
                MetricLabel { title: i18n("Cache write tokens"); value: panel.summary.cacheWriteTokens; count: true }
            }
            PlasmaComponents.Button {
                objectName: "viewSessionHistoryButton"
                visible: panel.presentationMode === "summary"
                text: i18n("View session history")
                icon.name: "view-history"
                onClicked: panel.openSessionHistory()
            }
        }
    }

    QQC2.ComboBox {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled() && panel.presentationMode !== "summary"
        model: panel.presentationMode === "analysis"
            ? [i18n("By day"), i18n("By model"), i18n("By project")]
            : [i18n("Sessions"), i18n("Daily usage")]
        currentIndex: panel.presentationMode === "analysis"
            ? panel.breakdownIndex : panel.historyDisplayIndex
        Accessible.name: i18n("Session breakdown")
        onActivated: {
            if (panel.presentationMode === "analysis") panel.breakdownIndex = currentIndex;
            else panel.historyDisplayIndex = currentIndex;
        }
    }

    SessionSection {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled() && panel.showDaily && panel.dailyRows.length > 0
        contentItem: ColumnLayout {
            Kirigami.Heading {
                Layout.fillWidth: true
                level: 5
                text: i18n("Daily usage")
            }
            ListView {
                id: dailyList
                objectName: "sessionDailyList"
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
                            text: panel.dayText(dailyRow.modelData.localDate)
                            font.bold: true
                        }
                        PlasmaComponents.Label {
                            text: i18n("%1 sessions",
                                panel.countText(dailyRow.modelData.sessionCount))
                        }
                    }
                    PlasmaComponents.Label {
                        id: dailyUsageValue
                        activeFocusOnTab: true
                        Accessible.name: i18n("Input tokens: %1 · Output tokens: %2",
                            panel.countText(dailyRow.modelData.inputTokens),
                            panel.countText(dailyRow.modelData.outputTokens))
                        QQC2.ToolTip.visible: dailyUsageValueHover.hovered || activeFocus
                        QQC2.ToolTip.text: Accessible.name
                        HoverHandler { id: dailyUsageValueHover }
                        Layout.fillWidth: true
                        text: i18n("Input %1 · output %2",
                            panel.tokenText(dailyRow.modelData.inputTokens),
                            panel.tokenText(dailyRow.modelData.outputTokens))
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
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

    SessionSection {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
            && panel.presentationMode === "analysis" && panel.breakdownIndex === 1
            && (panel.summary.modelBreakdown || []).length > 0
        contentItem: ColumnLayout {
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
                        * Kirigami.Units.gridUnit * 3,
                    Kirigami.Units.gridUnit * 7)
                clip: true
                model: panel.summary.modelBreakdown || []
                spacing: Kirigami.Units.smallSpacing
                Accessible.name: i18n("Token usage by model")

                delegate: ColumnLayout {
                    id: modelBreakdownRow
                    required property var modelData
                    width: modelBreakdownList.width
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: modelBreakdownRow.modelData.modelId || i18n("Model unavailable")
                        elide: Text.ElideRight
                    }
                    PlasmaComponents.Label {
                        id: modelUsageValue
                        activeFocusOnTab: true
                        Accessible.name: i18n("Input tokens: %1 · Output tokens: %2",
                            panel.countText(modelBreakdownRow.modelData.inputTokens),
                            panel.countText(modelBreakdownRow.modelData.outputTokens))
                        QQC2.ToolTip.visible: modelUsageValueHover.hovered || activeFocus
                        QQC2.ToolTip.text: Accessible.name
                        HoverHandler { id: modelUsageValueHover }
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: i18n("%1 events · input %2 · output %3",
                            panel.countText(modelBreakdownRow.modelData.eventCount),
                            panel.tokenText(modelBreakdownRow.modelData.inputTokens),
                            panel.tokenText(modelBreakdownRow.modelData.outputTokens))
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
    }

    SessionSection {
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
            && panel.presentationMode === "analysis" && panel.breakdownIndex === 2
            && (panel.summary.projectBreakdown || []).length > 0
        contentItem: ColumnLayout {
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
                        * Kirigami.Units.gridUnit * 3,
                    Kirigami.Units.gridUnit * 7)
                clip: true
                model: panel.summary.projectBreakdown || []
                spacing: Kirigami.Units.smallSpacing
                Accessible.name: i18n("Token usage by project")

                delegate: ColumnLayout {
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
                        id: projectUsageValue
                        activeFocusOnTab: true
                        Accessible.name: i18n("Input tokens: %1 · Output tokens: %2",
                            panel.countText(projectBreakdownRow.modelData.inputTokens),
                            panel.countText(projectBreakdownRow.modelData.outputTokens))
                        QQC2.ToolTip.visible: projectUsageValueHover.hovered || activeFocus
                        QQC2.ToolTip.text: Accessible.name
                        HoverHandler { id: projectUsageValueHover }
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: i18n("%1 sessions · input %2 · output %3",
                            panel.countText(projectBreakdownRow.modelData.sessionCount),
                            panel.tokenText(projectBreakdownRow.modelData.inputTokens),
                            panel.tokenText(projectBreakdownRow.modelData.outputTokens))
                        color: Kirigami.Theme.disabledTextColor
                    }
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
        objectName: "sessionHistoryList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Kirigami.Units.gridUnit * 10
        clip: true
        visible: panel.selectedToolEnabled() && panel.presentationMode === "history"
            && panel.historyDisplayIndex === 0
        model: panel.sessionRows
        spacing: Kirigami.Units.largeSpacing
        Accessible.name: i18n("Coding sessions")

        delegate: QQC2.ItemDelegate {
            id: sessionDelegate
            required property var modelData
            width: sessionList.width
            activeFocusOnTab: true
            padding: Kirigami.Units.largeSpacing
            background: Rectangle {
                radius: Kirigami.Units.smallSpacing
                color: sessionDelegate.hovered || sessionDelegate.down
                    ? Kirigami.Theme.alternateBackgroundColor : Kirigami.Theme.backgroundColor
                border.width: sessionDelegate.activeFocus ? 2 : 1
                border.color: sessionDelegate.activeFocus
                    || panel.selectedSessionId === sessionDelegate.modelData.sessionId
                    ? Kirigami.Theme.highlightColor : Qt.alpha(Kirigami.Theme.textColor, 0.18)
            }
            Accessible.name: i18n("Session ending %1, %2 events",
                panel.dateText(modelData.lastSeenAtUtc), panel.countText(modelData.eventCount))
            Accessible.description: panel.coverageText(modelData.coverage)
            onClicked: {
                panel.selectedSessionId = panel.selectedSessionId === modelData.sessionId
                    ? "" : modelData.sessionId;
                panel.persistState();
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                GridLayout {
                    Layout.fillWidth: true
                    columns: width < Kirigami.Units.gridUnit * 40 ? 1 : 2
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("%1 · %2 events",
                            panel.dateText(sessionDelegate.modelData.lastSeenAtUtc),
                            panel.countText(sessionDelegate.modelData.eventCount))
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: panel.coverageText(sessionDelegate.modelData.coverage)
                        color: sessionDelegate.modelData.coverage === "complete"
                            ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                    }
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Input %1 · Output %2",
                        panel.tokenText(sessionDelegate.modelData.inputTokens),
                        panel.tokenText(sessionDelegate.modelData.outputTokens))
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
                            panel.fullDateText(sessionDelegate.modelData.startedAtUtc),
                            panel.fullDateText(sessionDelegate.modelData.lastSeenAtUtc))
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width < Kirigami.Units.gridUnit * 40 ? 1 : 2
                        MetricLabel { title: i18n("Input tokens"); value: sessionDelegate.modelData.inputTokens; count: true }
                        MetricLabel { title: i18n("Output tokens"); value: sessionDelegate.modelData.outputTokens; count: true }
                        MetricLabel { title: i18n("Cache read tokens"); value: sessionDelegate.modelData.cacheReadTokens; count: true }
                        MetricLabel { title: i18n("Cache write tokens"); value: sessionDelegate.modelData.cacheWriteTokens; count: true }
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
        visible: panel.selectedToolEnabled() && panel.presentationMode === "history"
            && panel.historyDisplayIndex === 0
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
