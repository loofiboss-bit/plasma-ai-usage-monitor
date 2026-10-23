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
    property var selectedModelFilter: ({})
    property string projectSearch: ""
    property string modelSearch: ""
    property string queryErrorKey: ""
    property string projectErrorKey: ""
    property var pendingQueryIds: []
    property bool summaryCurrent: false
    readonly property bool hasNextPage: summaryCurrent && !loading && queryErrorText === ""
        && pageOffset + sessionRows.length < Number(summary.sessionCount || 0)
    property int selectedRangeIndex: 1
    property int pageOffset: 0
    property string selectedSessionId: ""
    property var projectRows: []
    property var sessionRows: []
    property var dailyRows: []
    property var summary: ({})
    property bool loading: false
    property string exportState: "idle"
    property string exportErrorKey: ""
    property bool exportDialogOpen: false
    property var pendingExportSelection: null
    property string customStartDate: ""
    property string customEndDate: ""
    property string customRangeErrorKey: ""
    readonly property bool exportBusy: exportDialogOpen || exportState === "writing"
    readonly property string statusText: exportState === "writing"
        ? i18n("Writing redacted export…")
        : exportState === "saved" ? i18n("Redacted session export saved.")
        : exportState === "error" ? exportErrorText(exportErrorKey) : ""
    readonly property string queryErrorText: (queryErrorKey || projectErrorKey) === "session-database-unavailable"
        ? i18n("The local session database is unavailable. Try again.")
        : (queryErrorKey || projectErrorKey) !== "" ? i18n("Session data could not be loaded. Try again.") : ""
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
                                   string sessionId, int pageOffset, var modelFilter)
    signal sessionHistoryRangeRequested(var selection)
    signal sessionDayReturnRequested()
    signal filtersChanged(string toolId, string projectId, int rangeDays)
    Accessible.role: Accessible.Pane
    Accessible.name: i18n("Local coding session history")

    MediaLocalSessionModel {
        id: mediaSessionModel
        toolId: panel.activeToolId
    }

    readonly property string activeToolId: fixedToolId.length > 0
        ? fixedToolId : selectedToolId
    readonly property int rangeDays: selectedRangeIndex === 3
        ? Math.max(1, customRangeDayCount())
        : ([1, 7, 30][selectedRangeIndex] || 7)

    function localDateString(date) {
        return String(date.getFullYear()).padStart(4, "0") + "-"
            + String(date.getMonth() + 1).padStart(2, "0") + "-"
            + String(date.getDate()).padStart(2, "0");
    }

    function parseLocalDate(value) {
        var parts = /^(\d{4})-(\d{2})-(\d{2})$/.exec(String(value || ""));
        if (!parts) return null;
        var year = Number(parts[1]);
        var month = Number(parts[2]);
        var day = Number(parts[3]);
        var date = new Date(year, month - 1, day);
        if (year < 100) date.setFullYear(year);
        return date.getFullYear() === year && date.getMonth() === month - 1
            && date.getDate() === day ? date : null;
    }

    function ensureCustomDates() {
        if (parseLocalDate(customStartDate) && parseLocalDate(customEndDate)) return;
        var end = new Date();
        end.setHours(0, 0, 0, 0);
        var start = new Date(end.getTime());
        start.setDate(start.getDate() - 6);
        customStartDate = localDateString(start);
        customEndDate = localDateString(end);
    }

    function customRangeProblem() {
        var from = parseLocalDate(customStartDate);
        var through = parseLocalDate(customEndDate);
        if (!from || !through) return "invalid-date";
        if (from > through) return "range-order";
        var today = new Date();
        today.setHours(0, 0, 0, 0);
        if (through > today) return "range-future";
        var retentionDays = sessionModel && Number(sessionModel.retentionDays) > 0
            ? Math.floor(Number(sessionModel.retentionDays)) : 90;
        var earliest = new Date(today.getTime());
        earliest.setDate(earliest.getDate() - retentionDays + 1);
        if (from < earliest) return "range-retention";
        return "";
    }

    function customRangeErrorText() {
        if (customRangeErrorKey === "invalid-date")
            return i18n("Enter both dates in YYYY-MM-DD format.");
        if (customRangeErrorKey === "range-order")
            return i18n("The start date must be on or before the end date.");
        if (customRangeErrorKey === "range-future")
            return i18n("The end date cannot be in the future.");
        if (customRangeErrorKey === "range-retention")
            return i18n("Choose dates within the configured history retention period.");
        return "";
    }

    function customRangeDayCount() {
        var from = parseLocalDate(customStartDate);
        var through = parseLocalDate(customEndDate);
        if (!from || !through || from > through) return 0;
        var days = 0;
        for (var date = new Date(from.getTime()); date <= through && days <= 3650;
             date.setDate(date.getDate() + 1)) ++days;
        return days;
    }

    function localRange() {
        var now = new Date();
        if (selectedRangeIndex === 3) {
            var customFrom = parseLocalDate(customStartDate);
            var customThrough = parseLocalDate(customEndDate);
            if (!customFrom || !customThrough) return { from: "", to: "" };
            var customTo = new Date(customThrough.getTime());
            customTo.setDate(customTo.getDate() + 1);
            customTo.setHours(0, 0, 0, 0);
            var today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
            return {
                from: customFrom.toISOString(),
                to: customThrough.getTime() === today.getTime()
                    ? now.toISOString() : customTo.toISOString()
            };
        }
        var from = new Date(now.getTime());
        from.setHours(0, 0, 0, 0);
        from.setDate(from.getDate() - rangeDays + 1);
        return { from: from.toISOString(), to: now.toISOString() };
    }

    function persistState() {
        if (!sessionState || restoringState) return;
        sessionState.toolId = selectedToolId;
        sessionState.projectId = selectedProjectId;
        sessionState.modelFilter = selectedModelFilter;
        sessionState.projectSearch = projectSearch;
        sessionState.modelSearch = modelSearch;
        sessionState.historyDisplayIndex = historyDisplayIndex;
        sessionState.breakdownIndex = breakdownIndex;
        sessionState.rangeIndex = selectedRangeIndex;
        sessionState.rangeMode = selectedRangeIndex === 3 ? "custom" : "preset";
        sessionState.customStartDate = customStartDate;
        sessionState.customEndDate = customEndDate;
        sessionState.pageOffset = pageOffset;
        sessionState.sessionId = selectedSessionId;
    }

    function restoreState() {
        if (!sessionState) return;
        restoringState = true;
        selectedToolId = String(sessionState.toolId || initialToolId || selectedToolId);
        selectedProjectId = String(sessionState.projectId || initialProjectId || "");
        selectedModelFilter = sessionState.modelFilter || ({});
        projectSearch = String(sessionState.projectSearch || "");
        modelSearch = String(sessionState.modelSearch || "");
        historyDisplayIndex = Number(sessionState.historyDisplayIndex) === 1 ? 1 : 0;
        breakdownIndex = Math.max(0, Math.min(2, Number(sessionState.breakdownIndex || 0)));
        selectedRangeIndex = Math.max(0, Math.min(3,
            Number(sessionState.rangeIndex === undefined
                ? ([1, 7, 30].indexOf(Number(initialRangeDays)) < 0 ? 1
                    : [1, 7, 30].indexOf(Number(initialRangeDays)))
                : sessionState.rangeIndex)));
        customStartDate = String(sessionState.customStartDate || "");
        customEndDate = String(sessionState.customEndDate || "");
        if (sessionState.rangeMode === "custom" || selectedRangeIndex === 3) {
            selectedRangeIndex = 3;
            ensureCustomDates();
        }
        customRangeErrorKey = selectedRangeIndex === 3 ? customRangeProblem() : "";
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

    function projectMatchesSearch(alias, anonymous) {
        var search = projectSearch.trim().toLocaleLowerCase();
        return !search || (alias + " " + anonymous).toLocaleLowerCase().indexOf(search) >= 0;
    }

    function matchingProjectCount() {
        var count = 0;
        var ordinal = 0;
        for (var i = 0; i < projectRows.length; ++i) {
            var row = projectRows[i] || {};
            if (row.toolId !== activeToolId) continue;
            if (projectMatchesSearch(String(row.alias || "").trim(),
                    i18n("Unnamed project %1", ++ordinal))) ++count;
        }
        return count;
    }

    function displayedProjects() {
        var rows = [{ label: i18n("All projects"), projectId: "" }];
        var ordinal = 0;
        for (var i = 0; i < projectRows.length; ++i) {
            var row = projectRows[i] || {};
            if (row.toolId !== activeToolId) continue;
            var anonymous = i18n("Unnamed project %1", ++ordinal);
            var alias = String(row.alias || "").trim();
            if (!projectMatchesSearch(alias, anonymous)
                    && String(row.projectId || "") !== selectedProjectId) continue;
            rows.push({ label: alias || anonymous, projectId: String(row.projectId || "") });
        }
        return rows;
    }

    function modelMatchesSearch(name) {
        var search = modelSearch.trim().toLocaleLowerCase();
        return !search || (name || i18n("Unknown model")).toLocaleLowerCase().indexOf(search) >= 0;
    }

    function matchingModelCount() {
        return (summary.modelOptions || []).filter(function(row) {
            return panel.modelMatchesSearch(String(row.modelId || ""));
        }).length;
    }

    function modelOptions() {
        var rows = [{ label: i18n("All models"), filter: ({}) }];
        var options = summary.modelOptions || [];
        var selected = selectedModelFilter.modelId;
        var selectedIncluded = selected === undefined;
        for (var i = 0; i < options.length; ++i) {
            var name = String(options[i].modelId || "");
            if (!modelMatchesSearch(name) && name !== selected) continue;
            rows.push({ label: name || i18n("Unknown model"), filter: {modelId: name} });
            if (name === selected) selectedIncluded = true;
        }
        if (!selectedIncluded)
            rows.push({ label: selected || i18n("Unknown model"), filter: {modelId: selected} });
        return rows;
    }

    function clearFilters() {
        selectedProjectId = "";
        selectedModelFilter = ({});
        projectSearch = "";
        modelSearch = "";
        filtersUpdated();
    }

    function emptyHistoryText() {
        return selectedProjectId !== "" || selectedModelFilter.modelId !== undefined
            ? i18n("No sessions match the active filters. Clear filters or choose a different period.")
            : i18n("No sessions were recorded for this period.");
    }

    function modelIndex() {
        if (selectedModelFilter.modelId === undefined) return 0;
        var rows = modelOptions();
        for (var i = 1; i < rows.length; ++i)
            if (rows[i].filter.modelId === selectedModelFilter.modelId) return i;
        return -1;
    }

    function filtersUpdated() {
        pageOffset = 0;
        selectedSessionId = "";
        persistState();
        refresh();
        filtersChanged(activeToolId, selectedProjectId, rangeDays);
    }

    function completeQuery(requestId) {
        pendingQueryIds = pendingQueryIds.filter(function(id) { return id !== requestId; });
        loading = pendingQueryIds.length > 0;
        if (!loading && summaryCurrent && queryErrorText === "") {
            var lastOffset = Math.max(0, Math.floor((Number(summary.sessionCount || 0) - 1) / 100) * 100);
            if (pageOffset > lastOffset) {
                pageOffset = lastOffset;
                selectedSessionId = "";
                persistState();
                refresh();
            }
        }
    }

    function drillDown(projectId, modelFilter) {
        persistState();
        if (selectedRangeIndex === 3) {
            sessionHistoryRangeRequested({
                toolId: activeToolId,
                projectId: projectId,
                modelFilter: modelFilter || ({}),
                startDate: customStartDate,
                endDate: customEndDate,
                dayDrilldown: false
            });
            return;
        }
        sessionHistoryRequested(activeToolId, projectId, rangeDays, "", 0, modelFilter);
    }

    function openDay(localDate) {
        if (!parseLocalDate(localDate)) return;
        if (sessionState) {
            sessionState.dayFocusDate = localDate;
            sessionState.dailyScrollY = Number(dailyList.contentY || 0);
        }
        persistState();
        sessionHistoryRangeRequested({
            toolId: activeToolId,
            projectId: selectedProjectId,
            modelFilter: selectedModelFilter,
            startDate: localDate,
            endDate: localDate,
            dayDrilldown: true
        });
    }

    function restoreDailyFocus() {
        if (!sessionState || !sessionState.dayFocusDate || !dailyList.visible) return;
        var index = -1;
        for (var i = 0; i < dailyRows.length; ++i) {
            if (String(dailyRows[i].localDate || "") === String(sessionState.dayFocusDate)) {
                index = i;
                break;
            }
        }
        if (index < 0) return;
        dailyList.positionViewAtIndex(index, ListView.Contain);
        dailyList.forceLayout();
        Qt.callLater(function() {
            var savedY = Math.max(0, Number(sessionState.dailyScrollY || 0));
            var maxY = Math.max(0, dailyList.contentHeight - dailyList.height);
            dailyList.contentY = Math.min(savedY, maxY);
            var row = dailyList.itemAtIndex(index);
            if (row) row.forceActiveFocus(Qt.TabFocusReason);
        });
    }

    function restoreBreakdownFocus() {
        if (!sessionState || presentationMode !== "analysis") return;
        var key = String(sessionState.focusKey || "");
        var list = breakdownIndex === 1 && key.indexOf("model:") === 0 ? modelBreakdownList
            : breakdownIndex === 2 && key.indexOf("project:") === 0 ? projectBreakdownList : null;
        if (!list || list.count === 0) return;
        var index = Math.max(0, Math.min(list.count - 1, Number(sessionState.focusIndex || 0)));
        list.positionViewAtIndex(index, ListView.Contain);
        list.forceLayout();
        Qt.callLater(function() {
            var row = list.itemAtIndex(index);
            if (row) row.focusAction();
        });
    }

    function projectIndex() {
        var rows = displayedProjects();
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].projectId === selectedProjectId) return i;
        }
        return selectedProjectId === "" ? 0 : -1;
    }

    function dateText(value) {
        var time = Date.parse(value || "");
        return Number.isFinite(time) ? Utils.formatWhen(time, "relative")
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
                                selectedSessionId, pageOffset, selectedModelFilter);
    }

    function coverageText(value) {
        if (value === "reported") return i18n("Reported usage data");
        if (value === "complete") return i18n("Complete coverage");
        if (value === "partial") return i18n("Partial coverage");
        if (value === "partial-counter-reset") return i18n("Partial after a counter reset");
        if (value === "conflict") return i18n("Conflicting records");
        if (value === "unverified_lineage") return i18n("Unverified fork or agent history");
        if (value === "partial-retention") return i18n("Partial history after retention cleanup");
        if (value === "partial-clear-rebaseline") return i18n("Partial history after clearing and rebuilding usage counters");
        if (value === "partial-identity") return i18n("Partial history because session identity is uncertain");
        if (value === "partial-lineage") return i18n("Partial history because fork or agent history is uncertain");
        if (value === "partial-usage") return i18n("Partial history because some usage records are unavailable");
        if (value === "partial-conflict") return i18n("Partial history because records conflict");
        return i18n("Coverage unavailable");
    }

    function projectLabel(projectId) {
        var ordinal = 0;
        for (var i = 0; i < projectRows.length; ++i) {
            if (projectRows[i].toolId !== activeToolId) continue;
            ++ordinal;
            if (projectRows[i].projectId === projectId) {
                var alias = String(projectRows[i].alias || "").trim();
                return alias || i18n("Unnamed project %1", ordinal);
            }
        }
        return i18n("Project unavailable");
    }

    function modelNames(rows) {
        var names = [];
        rows = rows || [];
        for (var i = 0; i < rows.length; ++i) {
            var name = String(rows[i].modelId || "") || i18n("Unknown model");
            if (names.indexOf(name) < 0) names.push(name);
        }
        return names;
    }

    function loadProjects() {
        if (!activeSessionModel || typeof activeSessionModel.requestProjects !== "function") return;
        if (!selectedToolEnabled()) {
            projectRows = [];
            return;
        }
        projectErrorKey = "";
        projectsRequestId = "session-projects-" + requestPrefix
            + "-" + (++requestGeneration);
        activeSessionModel.requestProjects(projectsRequestId);
    }

    function refresh() {
        if (selectedRangeIndex === 3) {
            customRangeErrorKey = customRangeProblem();
            if (customRangeErrorKey !== "") {
                ++requestGeneration;
                pendingQueryIds = [];
                loading = false;
                summaryCurrent = false;
                sessionRows = [];
                dailyRows = [];
                summary = ({});
                return;
            }
        }
        if (!sessionModel || !selectedToolEnabled()) {
            loading = false;
            pendingQueryIds = [];
            summaryCurrent = false;
            summaryRequestId = "";
            sessionsRequestId = "";
            dailyRequestId = "";
            queryErrorKey = "";
            sessionRows = [];
            dailyRows = [];
            summary = ({});
            return;
        }
        var range = localRange();
        var generation = ++requestGeneration;
        summaryRequestId = "session-summary-" + requestPrefix + "-" + generation;
        sessionsRequestId = "session-list-" + requestPrefix + "-" + generation;
        dailyRequestId = "session-daily-" + requestPrefix + "-" + generation;
        pendingQueryIds = [summaryRequestId, sessionsRequestId, dailyRequestId];
        loading = true;
        summaryCurrent = false;
        queryErrorKey = "";
        sessionRows = [];
        dailyRows = [];
        summary = ({});
        activeSessionModel.requestSummary(summaryRequestId, activeToolId,
            range.from, range.to, selectedProjectId, selectedModelFilter);
        activeSessionModel.requestSessions(sessionsRequestId, activeToolId,
            range.from, range.to, selectedProjectId, pageOffset, 100, selectedModelFilter);
        activeSessionModel.requestDaily(dailyRequestId, activeToolId,
            range.from, range.to, selectedProjectId, "", selectedModelFilter);
    }

    function exportErrorText(key) {
        if (key === "session-database-unavailable")
            return i18n("The local session database is unavailable. Try exporting again.");
        if (key === "session-export-destination-invalid" || key === "session-export-write-failed")
            return i18n("The export could not be saved. Choose a writable file and try again.");
        return i18n("Session export could not be completed. Try again.");
    }

    function prepareExport(format) {
        if (exportBusy || !selectedToolEnabled()) return false;
        if (selectedRangeIndex === 3 && customRangeProblem() !== "") return false;
        var range = localRange();
        pendingExportSelection = {
            toolId: activeToolId, projectId: selectedProjectId,
            modelFilter: selectedModelFilter.modelId === undefined
                ? ({}) : ({modelId: selectedModelFilter.modelId}),
            from: range.from, to: range.to, format: format
        };
        pendingExportFormat = format;
        exportDialogOpen = true;
        return true;
    }

    function openExport(format) {
        if (!prepareExport(format)) return;
        exportDialog.nameFilters = format === "csv"
            ? [i18n("CSV files (*.csv)")] : [i18n("JSON files (*.json)")];
        exportDialog.currentFile = "coding-sessions." + format;
        exportDialog.open();
    }

    function beginExport(path) {
        if (exportState === "writing") return false;
        if (!pendingExportSelection && !prepareExport(pendingExportFormat)) return false;
        var selection = pendingExportSelection;
        pendingExportSelection = null;
        exportDialogOpen = false;
        if (!activeSessionModel || !(selection.toolId === "codex-cli"
                ? activeSessionModel.codexEnabled : activeSessionModel.claudeCodeEnabled)) {
            exportState = "error";
            exportErrorKey = "session-invalid-query";
            return false;
        }
        exportRequestId = "session-export-" + requestPrefix
            + "-" + (++requestGeneration);
        exportErrorKey = "";
        exportState = "writing";
        activeSessionModel.requestExport(exportRequestId, selection.toolId,
            selection.from, selection.to, selection.projectId,
            selection.format, path, selection.modelFilter);
        return true;
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
                if (requestId !== panel.sessionsRequestId || panel.selectedSessionId === ""
                        || !sessionList.visible) return;
                sessionList.positionViewAtIndex(selectedIndex, ListView.Contain);
                sessionList.forceLayout();
                Qt.callLater(function() {
                    if (requestId !== panel.sessionsRequestId) return;
                    var delegate = sessionList.itemAtIndex(selectedIndex);
                    if (delegate) delegate.forceActiveFocus(Qt.TabFocusReason);
                });
            });
        }
        completeQuery(requestId);
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
            panel.projectErrorKey = "";
            if (panel.selectedProjectId !== ""
                    && !panel.projectRows.some(function(row) { return row.projectId === panel.selectedProjectId && row.toolId === panel.activeToolId; })) {
                panel.selectedProjectId = "";
                panel.filtersUpdated();
            }
        }

        function onSummaryReady(requestId, payload) {
            if (requestId !== panel.summaryRequestId) return;
            panel.summary = payload || {};
            panel.summaryCurrent = true;
            panel.completeQuery(requestId);
        }

        function onSessionsReady(requestId, rows) {
            panel.acceptSessionRows(requestId, rows);
        }

        function onDailyReady(requestId, rows) {
            if (requestId !== panel.dailyRequestId) return;
            panel.dailyRows = rows || [];
            panel.completeQuery(requestId);
        }

        function onQueryFailed(requestId, errorKey) {
            if (requestId === panel.projectsRequestId) {
                panel.projectErrorKey = errorKey || "session-query-failed";
                return;
            }
            if (panel.pendingQueryIds.indexOf(requestId) < 0) return;
            panel.queryErrorKey = errorKey || "session-query-failed";
            panel.completeQuery(requestId);
        }

        function onExportReady(requestId, errorKey) {
            if (requestId !== panel.exportRequestId) return;
            panel.exportErrorKey = errorKey;
            panel.exportState = errorKey === "" ? "saved" : "error";
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
        visible: panel.presentationMode !== "summary"
        contentItem: GridLayout {
            columns: width < Kirigami.Units.gridUnit * 40 ? 1 : 2

            QQC2.ComboBox {
                objectName: "sessionToolPicker"
                Layout.fillWidth: true
                visible: panel.fixedToolId === ""
                model: panel.toolOptions()
                textRole: "label"
                valueRole: "value"
                currentIndex: panel.activeToolId === "claude-code" ? 1 : 0
                Accessible.name: i18n("Session tool")
                onActivated: {
                    panel.selectedToolId = currentValue;
                }
            }

            QQC2.ComboBox {
                Layout.fillWidth: true
                model: panel.presentationMode === "history"
                    ? [i18n("Today"), i18n("7 days"), i18n("30 days"), i18n("Custom")]
                    : [i18n("Today"), i18n("7 days"), i18n("30 days")]
                currentIndex: panel.selectedRangeIndex
                Accessible.name: i18n("Session period")
                onActivated: {
                    panel.selectedRangeIndex = currentIndex;
                    if (currentIndex === 3) {
                        panel.ensureCustomDates();
                        panel.customRangeErrorKey = panel.customRangeProblem();
                        if (panel.customRangeErrorKey === "")
                            panel.filtersUpdated();
                        else
                            panel.persistState();
                    } else {
                        panel.customRangeErrorKey = "";
                        panel.filtersUpdated();
                    }
                }
            }

            RowLayout {
                objectName: "sessionCustomRange"
                Layout.fillWidth: true
                visible: panel.presentationMode === "history"
                    && panel.selectedRangeIndex === 3
                QQC2.TextField {
                    objectName: "sessionCustomStartDate"
                    Layout.fillWidth: true
                    placeholderText: i18n("Start date (YYYY-MM-DD)")
                    Accessible.name: placeholderText
                    inputMask: "0000-00-00;_"
                    text: panel.customStartDate
                    onTextEdited: panel.customStartDate = text
                    onEditingFinished: {
                        panel.customRangeErrorKey = panel.customRangeProblem();
                        if (panel.customRangeErrorKey === "")
                            panel.filtersUpdated();
                        else
                            panel.persistState();
                    }
                }
                QQC2.TextField {
                    objectName: "sessionCustomEndDate"
                    Layout.fillWidth: true
                    placeholderText: i18n("End date (YYYY-MM-DD)")
                    Accessible.name: placeholderText
                    inputMask: "0000-00-00;_"
                    text: panel.customEndDate
                    onTextEdited: panel.customEndDate = text
                    onEditingFinished: {
                        panel.customRangeErrorKey = panel.customRangeProblem();
                        if (panel.customRangeErrorKey === "")
                            panel.filtersUpdated();
                        else
                            panel.persistState();
                    }
                }
            }

            PlasmaComponents.Label {
                objectName: "sessionCustomRangeError"
                Layout.fillWidth: true
                visible: panel.presentationMode === "history"
                    && panel.selectedRangeIndex === 3
                    && panel.customRangeErrorKey !== ""
                text: panel.customRangeErrorText()
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.WordWrap
                Accessible.name: text
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: panel.presentationMode === "history"
                    && panel.selectedRangeIndex === 3
                text: i18n("Available within the configured retention period. Initial local import covers up to 30 days.")
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.WordWrap
            }

            QQC2.TextField {
                objectName: "sessionProjectSearch"
                Layout.fillWidth: true
                placeholderText: i18n("Search projects")
                Accessible.name: placeholderText
                text: panel.projectSearch
                onTextEdited: { panel.projectSearch = text; panel.persistState(); }
            }

            QQC2.ComboBox {
                objectName: "sessionProjectPicker"
                Layout.fillWidth: true
                model: panel.displayedProjects()
                textRole: "label"
                valueRole: "projectId"
                currentIndex: panel.projectIndex()
                Accessible.name: i18n("Session project")
                onActivated: {
                    panel.selectedProjectId = currentValue || "";
                    panel.filtersUpdated();
                }
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                objectName: "sessionProjectSearchEmpty"
                visible: panel.projectSearch.trim() !== "" && panel.matchingProjectCount() === 0
                text: i18n("No projects match this search.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }

            QQC2.TextField {
                objectName: "sessionModelSearch"
                Layout.fillWidth: true
                placeholderText: i18n("Search models")
                Accessible.name: placeholderText
                text: panel.modelSearch
                onTextEdited: { panel.modelSearch = text; panel.persistState(); }
            }
            PlasmaComponents.Label {
                objectName: "sessionModelSearchEmpty"
                Layout.fillWidth: true
                visible: panel.modelSearch.trim() !== "" && panel.summaryCurrent
                    && panel.queryErrorText === "" && panel.matchingModelCount() === 0
                text: i18n("No models match this search. The active model filter is unchanged.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.ComboBox {
                objectName: "sessionModelPicker"
                Layout.fillWidth: true
                model: panel.modelOptions()
                textRole: "label"
                currentIndex: panel.modelIndex()
                Accessible.name: i18n("Session model")
                onActivated: {
                    panel.selectedModelFilter = model[currentIndex].filter;
                    panel.filtersUpdated();
                }
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                objectName: "sessionActiveFilters"
                visible: panel.selectedProjectId !== "" || panel.selectedModelFilter.modelId !== undefined
                text: i18n("Active filter: %1 · %2", panel.selectedProjectId ? panel.projectLabel(panel.selectedProjectId) : i18n("All projects"),
                    panel.selectedModelFilter.modelId === undefined ? i18n("All models") : panel.selectedModelFilter.modelId || i18n("Unknown model"))
                wrapMode: Text.WordWrap
            }
            PlasmaComponents.Button {
                text: i18n("Clear filters")
                objectName: "sessionClearFilters"
                visible: panel.selectedProjectId !== "" || panel.selectedModelFilter.modelId !== undefined
                    || panel.projectSearch !== "" || panel.modelSearch !== ""
                onClicked: panel.clearFilters()
            }

            PlasmaComponents.Button {
                objectName: "sessionDayDrilldownReturn"
                visible: panel.presentationMode === "history"
                    && !!(panel.sessionState && panel.sessionState.dayDrilldown)
                text: i18n("Back to daily overview")
                icon.name: "go-previous"
                activeFocusOnTab: true
                onClicked: panel.sessionDayReturnRequested()
            }

            PlasmaComponents.Button {
                Layout.fillWidth: true
                text: i18n("Export")
                icon.name: "document-export"
                activeFocusOnTab: true
                enabled: !panel.loading && panel.selectedToolEnabled() && !panel.exportBusy && panel.queryErrorText === ""
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
                columns: width < Kirigami.Units.gridUnit * 30 ? 1 : 3
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

    PlasmaComponents.Label {
        objectName: "sessionQueryError"
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled() && panel.queryErrorText !== ""
        text: i18n("Session data could not be loaded: %1", panel.queryErrorText)
        color: Kirigami.Theme.negativeTextColor
        wrapMode: Text.WordWrap
        Accessible.name: text
    }

    PlasmaComponents.Button {
        objectName: "sessionQueryRetry"
        visible: panel.queryErrorText !== ""
        text: i18n("Try again")
        onClicked: { panel.loadProjects(); panel.refresh(); }
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
            if (panel.presentationMode === "analysis") { panel.breakdownIndex = currentIndex; panel.persistState(); }
            else { panel.historyDisplayIndex = currentIndex; panel.persistState(); }
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
                onCountChanged: Qt.callLater(panel.restoreDailyFocus)
                onVisibleChanged: if (visible) Qt.callLater(panel.restoreDailyFocus)
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(
                    panel.dailyRows.length * Kirigami.Units.gridUnit * 3,
                    Kirigami.Units.gridUnit * 7)
                Layout.minimumHeight: Kirigami.Units.gridUnit * 3
                clip: true
                model: panel.dailyRows
                spacing: Kirigami.Units.smallSpacing
                Accessible.name: i18n("Daily session token usage")
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                    policy: QQC2.ScrollBar.AsNeeded
                }

                delegate: QQC2.ItemDelegate {
                    id: dailyRow
                    objectName: "sessionDailyRow"
                    required property int index
                    required property var modelData
                    width: dailyList.width
                    activeFocusOnTab: true
                    Accessible.name: i18n("Open sessions for %1, %2 sessions",
                        panel.dayText(modelData.localDate),
                        panel.countText(modelData.sessionCount))
                    Accessible.description: i18n("Input %1, output %2",
                        panel.countText(modelData.inputTokens),
                        panel.countText(modelData.outputTokens))
                    onClicked: panel.openDay(String(modelData.localDate || ""))

                    contentItem: ColumnLayout {
                        spacing: 0
                        RowLayout {
                            Layout.fillWidth: true
                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                text: panel.dayText(dailyRow.modelData.localDate)
                                font.bold: true
                            }
                            PlasmaComponents.Label {
                                text: i18np("%1 session", "%1 sessions",
                                    Number(dailyRow.modelData.sessionCount || 0))
                            }
                        }
                        PlasmaComponents.Label {
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
    }

    PlasmaComponents.Label {
        objectName: "sessionDailyEmptyState"
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled() && panel.showDaily
            && !panel.loading && panel.dailyRows.length === 0
            && panel.queryErrorText === ""
        text: panel.selectedProjectId !== "" || panel.selectedModelFilter.modelId !== undefined
            ? i18n("No daily usage matches the active filters. Clear filters or choose a different period.")
            : i18n("No daily usage was recorded for this period.")
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        visible: panel.statusText !== ""
        text: panel.statusText
        wrapMode: Text.WordWrap
        objectName: "sessionExportStatus"
        color: panel.exportState === "error"
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
            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: Number(panel.summary.modelBreakdownTotalCount || 0) > (panel.summary.modelBreakdown || []).length
                text: i18n("Showing %1 of %2", (panel.summary.modelBreakdown || []).length, panel.summary.modelBreakdownTotalCount || 0)
            }
            ListView {
                id: modelBreakdownList
                onCountChanged: Qt.callLater(panel.restoreBreakdownFocus)
                onVisibleChanged: if (visible) Qt.callLater(panel.restoreBreakdownFocus)
                objectName: "sessionModelBreakdownList"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(
                    (panel.summary.modelBreakdown || []).length
                        * Kirigami.Units.gridUnit * 3,
                    Kirigami.Units.gridUnit * 7)
                clip: true
                model: panel.summary.modelBreakdown || []
                spacing: Kirigami.Units.smallSpacing
                Accessible.name: i18n("Token usage by model")
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                    policy: QQC2.ScrollBar.AsNeeded
                }

                delegate: ColumnLayout {
                    id: modelBreakdownRow
                    required property int index
                    required property var modelData
                    function focusAction() { modelBreakdownButton.forceActiveFocus(Qt.TabFocusReason); }
                    width: modelBreakdownList.width
                    QQC2.Button {
                        id: modelBreakdownButton
                        Layout.fillWidth: true
                        text: modelBreakdownRow.modelData.modelId || i18n("Unknown model")
                        onClicked: {
                            if (panel.sessionState) {
                                panel.sessionState.focusKey = "model:" + String(modelBreakdownRow.modelData.modelId || "");
                                panel.sessionState.focusIndex = modelBreakdownRow.index;
                            }
                            panel.drillDown(panel.selectedProjectId, {modelId: String(modelBreakdownRow.modelData.modelId || "")});
                        }
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
            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: Number(panel.summary.projectBreakdownTotalCount || 0) > (panel.summary.projectBreakdown || []).length
                text: i18n("Showing %1 of %2", (panel.summary.projectBreakdown || []).length, panel.summary.projectBreakdownTotalCount || 0)
            }
            ListView {
                id: projectBreakdownList
                onCountChanged: Qt.callLater(panel.restoreBreakdownFocus)
                onVisibleChanged: if (visible) Qt.callLater(panel.restoreBreakdownFocus)
                objectName: "sessionProjectBreakdownList"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(
                    (panel.summary.projectBreakdown || []).length
                        * Kirigami.Units.gridUnit * 3,
                    Kirigami.Units.gridUnit * 7)
                clip: true
                model: panel.summary.projectBreakdown || []
                spacing: Kirigami.Units.smallSpacing
                Accessible.name: i18n("Token usage by project")
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                    policy: QQC2.ScrollBar.AsNeeded
                }

                delegate: ColumnLayout {
                    id: projectBreakdownRow
                    required property int index
                    required property var modelData
                    function focusAction() { projectBreakdownButton.forceActiveFocus(Qt.TabFocusReason); }
                    width: projectBreakdownList.width
                    QQC2.Button {
                        id: projectBreakdownButton
                        Layout.fillWidth: true
                        text: panel.projectLabel(projectBreakdownRow.modelData.projectId)
                        onClicked: {
                            if (panel.sessionState) {
                                panel.sessionState.focusKey = "project:" + String(projectBreakdownRow.modelData.projectId || "");
                                panel.sessionState.focusIndex = projectBreakdownRow.index;
                            }
                            panel.drillDown(String(projectBreakdownRow.modelData.projectId || ""), panel.selectedModelFilter);
                        }
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

    PlasmaComponents.Label {
        objectName: "sessionModelEmptyState"
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
            && panel.presentationMode === "analysis" && panel.breakdownIndex === 1
            && !panel.loading && (panel.summary.modelBreakdown || []).length === 0
            && panel.queryErrorText === ""
        text: panel.selectedProjectId !== "" || panel.selectedModelFilter.modelId !== undefined
            ? i18n("No model usage matches the active filters. Clear filters or choose a different period.")
            : i18n("No model usage was recorded for this period.")
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }

    PlasmaComponents.Label {
        objectName: "sessionProjectEmptyState"
        Layout.fillWidth: true
        visible: panel.selectedToolEnabled()
            && panel.presentationMode === "analysis" && panel.breakdownIndex === 2
            && !panel.loading && (panel.summary.projectBreakdown || []).length === 0
            && panel.queryErrorText === ""
        text: panel.selectedProjectId !== "" || panel.selectedModelFilter.modelId !== undefined
            ? i18n("No project usage matches the active filters. Clear filters or choose a different period.")
            : i18n("No project usage was recorded for this period.")
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
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
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {
            policy: QQC2.ScrollBar.AsNeeded
        }

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
                        text: i18n("%1 · %2",
                            panel.dateText(sessionDelegate.modelData.lastSeenAtUtc),
                            i18np("%1 event", "%1 events",
                                Number(sessionDelegate.modelData.eventCount || 0)))
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: panel.coverageText(sessionDelegate.modelData.coverage)
                        color: sessionDelegate.modelData.coverage === "complete"
                            || sessionDelegate.modelData.coverage === "reported"
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
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: Number(sessionDelegate.modelData.modelBreakdownTotalCount || 0) > (sessionDelegate.modelData.modelBreakdown || []).length
                        text: i18n("Showing %1 of %2 models", (sessionDelegate.modelData.modelBreakdown || []).length, sessionDelegate.modelData.modelBreakdownTotalCount || 0)
                        wrapMode: Text.WordWrap
                    }
                    PlasmaComponents.Button {
                        text: i18n("Open this filter in History")
                        visible: panel.fixedToolId !== ""
                        activeFocusOnTab: true
                        Accessible.name: text
                        onClicked: panel.sessionHistoryRequested(
                            panel.activeToolId, panel.selectedProjectId, panel.rangeDays,
                            panel.selectedSessionId, panel.pageOffset, panel.selectedModelFilter)
                    }
                }
            }
        }

        PlasmaComponents.Label {
            anchors.centerIn: parent
            visible: !panel.loading && panel.sessionRows.length === 0
            text: panel.queryErrorText !== ""
                ? i18n("Session data could not be loaded: %1", panel.queryErrorText)
                : panel.emptyHistoryText()
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
            objectName: "sessionPageRangeLabel"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: panel.sessionRows.length > 0
                ? (panel.summaryCurrent
                    && Number(panel.summary.sessionCount || 0) >= panel.pageOffset + panel.sessionRows.length
                    ? i18n("Rows %1–%2 of %3", panel.pageOffset + 1,
                           panel.pageOffset + panel.sessionRows.length,
                           panel.countText(panel.summary.sessionCount))
                    : i18n("Rows %1–%2", panel.pageOffset + 1,
                           panel.pageOffset + panel.sessionRows.length))
                : i18n("No rows")
        }
        PlasmaComponents.Button {
            text: i18n("Next")
            objectName: "sessionNextPage"
            enabled: panel.hasNextPage
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
        onRejected: {
            panel.exportDialogOpen = false;
            panel.pendingExportSelection = null;
        }
    }

    onVisibleChanged: {
        if (visible) {
            loadProjects();
            refresh();
        }
    }

    onSelectedToolIdChanged: {
        if (fixedToolId === "" && !restoringState) {
            selectedProjectId = "";
            selectedModelFilter = ({});
            projectSearch = "";
            modelSearch = "";
            loadProjects();
            filtersUpdated();
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
