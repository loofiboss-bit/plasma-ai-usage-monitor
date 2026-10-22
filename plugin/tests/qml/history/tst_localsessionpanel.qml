import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "LocalSessionPanel"
    width: 1000
    height: 900
    visible: true
    when: windowShown

    QtObject {
        id: database
        property bool codexEnabled: true
        property bool claudeCodeEnabled: true
        property var requests: ({})
        property var filters: ({})
        signal sessionsReady(string requestId, var rows)
        signal summaryReady(string requestId, var payload)
        signal dailyReady(string requestId, var rows)
        signal projectsReady(string requestId, var rows)
        signal queryFailed(string requestId, string errorKey)
        signal exportReady(string requestId, string errorKey)
        signal importFinished(string toolId, int count, bool partial, string errorKey)
        property int exportCount: 0
        property var exportedSelection: ({})
        function requestProjects(id) { requests.projects = id; }
        function requestSummary(id, tool, from, to, project, filter) { requests.summary = id; filters.summary = filter; }
        function requestSessions(id, tool, from, to, project, offset, limit, filter) { requests.sessions = id; filters.sessions = filter; }
        function requestDaily(id, tool, from, to, project, zone, filter) { requests.daily = id; filters.daily = filter; }
        function requestExport(id, tool, from, to, project, format, path, filter) {
            filters.export = filter;
            ++exportCount;
            exportedSelection = {tool: tool, from: from, to: to, project: project, format: format, path: path, filter: filter};
        }
    }
    Component {
        id: panelComponent
        Components.LocalSessionPanel {
            width: 950
            height: 850
            presentationMode: "history"
            sessionModel: database
        }
    }
    property var panel: null
    function init() {
        database.exportCount = 0;
        panel = createTemporaryObject(panelComponent, testCase);
        verify(panel !== null);
        panel.refresh();
    }
    function finish(count, rows) {
        database.summaryReady(panel.summaryRequestId, {sessionCount: count, modelOptions: [{modelId: ""}, {modelId: "model-a"}]});
        database.sessionsReady(panel.sessionsRequestId, rows || []);
        database.dailyReady(panel.dailyRequestId, []);
    }
    function test_pagination_data() {
        return [0, 99, 100, 101, 200].map(function(count) { return {tag: String(count), count: count}; });
    }
    function test_pagination(data) {
        var rows = [];
        for (var i = 0; i < Math.min(100, data.count); ++i) rows.push({sessionId: String(i)});
        database.sessionsReady(panel.sessionsRequestId, rows);
        verify(!panel.hasNextPage);
        database.summaryReady(panel.summaryRequestId, {sessionCount: data.count});
        verify(panel.loading);
        database.dailyReady(panel.dailyRequestId, []);
        compare(panel.hasNextPage, data.count > 100);
        if (data.count === 200) {
            panel.pageOffset = 100;
            panel.refresh();
            verify(!panel.hasNextPage);
            finish(200, rows);
            verify(!panel.hasNextPage);
        }
    }
    function test_errors_retry_empty_and_stale() {
        var old = panel.summaryRequestId;
        database.queryFailed(old, "session-database-unavailable");
        verify(panel.queryErrorText.length > 0);
        database.sessionsReady(panel.sessionsRequestId, []);
        database.dailyReady(panel.dailyRequestId, []);
        verify(!panel.loading);
        verify(findChild(panel, "sessionQueryRetry").visible);
        panel.refresh();
        database.summaryReady(old, {sessionCount: 999});
        database.queryFailed(old, "session-query-failed");
        compare(panel.queryErrorKey, "");
        compare(panel.summaryCurrent, false);
        finish(0);
        verify(!panel.loading);
        compare(panel.queryErrorText, "");
        compare(panel.summary.sessionCount, 0);
    }
    function test_sql_and_invalid_query_errors_data() {
        return [{tag: "sql", key: "session-query-failed"}, {tag: "invalid", key: "session-invalid-query"}];
    }
    function test_sql_and_invalid_query_errors(data) {
        database.queryFailed(panel.sessionsRequestId, data.key);
        database.summaryReady(panel.summaryRequestId, {});
        database.dailyReady(panel.dailyRequestId, []);
        verify(!panel.loading);
        verify(panel.queryErrorText.indexOf(data.key) < 0);
    }
    function test_coverage() {
        var statuses = ["reported", "complete", "partial", "partial-counter-reset", "conflict", "unverified_lineage", "partial-retention", "partial-clear-rebaseline", "partial-identity", "partial-lineage", "partial-usage", "partial-conflict", "future-internal-code"];
        for (var i = 0; i < statuses.length; ++i) {
            verify(panel.coverageText(statuses[i]).length > 0);
            verify(panel.coverageText(statuses[i]) !== statuses[i]);
        }
        compare(panel.coverageText("future-internal-code"), panel.coverageText(""));
    }
    function test_project_search_all_rows_alias_and_empty() {
        var projects = [];
        for (var i = 0; i < 150; ++i) projects.push({toolId: "codex-cli", projectId: "p" + i, alias: i === 149 ? "My Workspace" : ""});
        panel.projectRows = projects;
        compare(panel.displayedProjects().length, 151);
        panel.projectSearch = "WORKSPACE";
        compare(panel.displayedProjects()[1].projectId, "p149");
        panel.projectSearch = "Unnamed project 150";
        compare(panel.displayedProjects()[1].projectId, "p149");
        panel.projectSearch = "missing";
        compare(panel.displayedProjects().length, 1);
        projects[149].alias = "Renamed";
        panel.projectRows = projects.slice();
        panel.projectSearch = "renamed";
        compare(panel.displayedProjects()[1].projectId, "p149");
        panel.selectedProjectId = "p149";
        panel.projectSearch = "missing";
        database.projectsReady(panel.projectsRequestId, projects);
        compare(panel.selectedProjectId, "p149");
    }
    function test_model_filter_all_requests_and_old_state() {
        panel.sessionState = {toolId: "codex-cli", rangeIndex: 1};
        panel.restoreState();
        compare(panel.selectedModelFilter.modelId, undefined);
        panel.selectedModelFilter = {modelId: ""};
        panel.pageOffset = 100;
        panel.selectedSessionId = "selected";
        panel.filtersUpdated();
        compare(panel.pageOffset, 0);
        compare(panel.selectedSessionId, "");
        compare(database.filters.summary.modelId, "");
        compare(database.filters.sessions.modelId, "");
        compare(database.filters.daily.modelId, "");
        panel.beginExport("/unused");
        compare(database.filters.export.modelId, "");
        finish(0);
        compare(panel.modelOptions().length, 3);
        compare(panel.modelIndex(), 1);
        panel.selectedModelFilter = {modelId: "model-a"};
        panel.persistState();
        panel.selectedModelFilter = ({});
        panel.restoreState();
        compare(panel.selectedModelFilter.modelId, "model-a");
    }
    function test_project_failure_retry_reloads_choices_and_ignores_stale_failure() {
        var old = panel.projectsRequestId;
        database.queryFailed(old, "session-query-failed");
        finish(0);
        verify(panel.queryErrorText.length > 0);
        findChild(panel, "sessionQueryRetry").clicked();
        verify(panel.projectsRequestId !== old);
        database.queryFailed(old, "session-database-unavailable");
        compare(panel.projectErrorKey, "");
        database.projectsReady(panel.projectsRequestId, [{toolId: "codex-cli", projectId: "restored", alias: "Restored"}]);
        finish(0);
        compare(panel.queryErrorText, "");
        compare(panel.displayedProjects()[1].projectId, "restored");
    }
    function test_offscreen_breakdown_focus_is_restored_data() {
        return [{tag: "model", kind: "model", breakdownIndex: 1}, {tag: "project", kind: "project", breakdownIndex: 2}];
    }
    function test_offscreen_breakdown_focus_is_restored(data) {
        var state = {toolId: "codex-cli", rangeIndex: 1, breakdownIndex: data.breakdownIndex};
        panel.sessionState = state;
        panel.presentationMode = "analysis";
        var rows = [];
        for (var i = 0; i < 100; ++i) rows.push({modelId: "m" + i, projectId: "p" + i, eventCount: 1, sessionCount: 1, inputTokens: 1, outputTokens: 1});
        var summary = {sessionCount: 100, modelBreakdown: rows, projectBreakdown: rows};
        database.summaryReady(panel.summaryRequestId, summary);
        var list = findChild(panel, data.kind === "model" ? "sessionModelBreakdownList" : "sessionProjectBreakdownList");
        list.positionViewAtIndex(50, ListView.Contain);
        list.forceLayout();
        tryVerify(function() { return list.itemAtIndex(50) !== null; });
        var row = list.itemAtIndex(50);
        row.focusAction();
        var button = row.children[0];
        button.clicked();
        compare(state.focusIndex, 50);
        var restored = createTemporaryObject(panelComponent, testCase, {sessionState: state, presentationMode: "analysis"});
        verify(restored !== null);
        database.summaryReady(restored.summaryRequestId, summary);
        var restoredList = findChild(restored, data.kind === "model" ? "sessionModelBreakdownList" : "sessionProjectBreakdownList");
        tryVerify(function() {
            var restoredRow = restoredList.itemAtIndex(50);
            return restoredRow !== null && restoredRow.children[0].activeFocus;
        });
        verify(restoredList.contentY > 0);
    }
    function test_model_search_complete_choices_and_pinned_selection() {
        var models = [{modelId: ""}];
        for (var i = 0; i < 150; ++i) models.push({modelId: "Model-" + i});
        database.summaryReady(panel.summaryRequestId, {sessionCount: 0, modelOptions: models});
        compare(panel.modelOptions().length, 152);
        panel.modelSearch = "MODEL-149";
        compare(panel.modelOptions().length, 2);
        compare(panel.modelOptions()[1].filter.modelId, "Model-149");
        compare(panel.selectedModelFilter.modelId, undefined);
        panel.selectedModelFilter = {modelId: "Model-42"};
        compare(panel.modelOptions().length, 3);
        compare(panel.modelOptions()[panel.modelIndex()].filter.modelId, "Model-42");
        panel.modelSearch = "no-such-model";
        compare(panel.matchingModelCount(), 0);
        verify(findChild(panel, "sessionModelSearchEmpty").visible);
        compare(panel.modelOptions()[panel.modelIndex()].filter.modelId, "Model-42");
        panel.selectedModelFilter = {modelId: ""};
        compare(panel.modelOptions()[panel.modelIndex()].label, panel.modelNames([{modelId: ""}])[0]);
    }
    function test_tool_switch_resets_selection_and_ignores_old_replies() {
        panel.selectedProjectId = "old-project";
        panel.selectedModelFilter = {modelId: "old-model"};
        panel.projectSearch = "old project";
        panel.modelSearch = "old model";
        panel.pageOffset = 100;
        panel.selectedSessionId = "old-session";
        var oldSummary = panel.summaryRequestId;
        var oldSessions = panel.sessionsRequestId;
        var oldProjects = panel.projectsRequestId;
        panel.selectedToolId = "claude-code";
        compare(panel.selectedProjectId, "");
        compare(panel.selectedModelFilter.modelId, undefined);
        compare(panel.projectSearch, "");
        compare(panel.modelSearch, "");
        compare(panel.pageOffset, 0);
        compare(panel.selectedSessionId, "");
        database.summaryReady(oldSummary, {sessionCount: 999});
        database.sessionsReady(oldSessions, [{sessionId: "old-session"}]);
        database.projectsReady(oldProjects, [{toolId: "codex-cli", projectId: "old-project"}]);
        compare(panel.summaryCurrent, false);
        compare(panel.sessionRows.length, 0);
    }
    function test_missing_selected_model_remains_explicit_until_reset() {
        panel.selectedModelFilter = {modelId: "missing-model"};
        panel.selectedRangeIndex = 0;
        panel.filtersUpdated();
        finish(0);
        compare(panel.selectedModelFilter.modelId, "missing-model");
        compare(panel.modelOptions()[panel.modelIndex()].label, "missing-model");
        verify(findChild(panel, "sessionActiveFilters").visible);
        verify(panel.emptyHistoryText().indexOf("active filters") >= 0);
        panel.historyDisplayIndex = 1;
        verify(findChild(panel, "sessionDailyEmptyState").visible);
        verify(findChild(panel, "sessionDailyEmptyState").text.indexOf("active filters") >= 0);
        findChild(panel, "sessionClearFilters").clicked();
        compare(panel.selectedModelFilter.modelId, undefined);
    }
    function test_restored_daily_view_and_model_search_with_old_state_default() {
        panel.sessionState = {toolId: "codex-cli", historyDisplayIndex: 1,
                              modelSearch: "Model", modelFilter: {modelId: "model-a"}};
        compare(panel.historyDisplayIndex, 1);
        compare(panel.modelSearch, "Model");
        verify(panel.showDaily);
        panel.persistState();
        compare(panel.sessionState.historyDisplayIndex, 1);
        panel.sessionState = {toolId: "codex-cli"};
        compare(panel.historyDisplayIndex, 0);
        compare(panel.modelSearch, "");
    }
    function test_retention_clamps_page_after_complete_generation_data() {
        return [{tag: "last-page", total: 101, offset: 100}, {tag: "empty", total: 0, offset: 0}];
    }
    function test_retention_clamps_page_after_complete_generation(data) {
        panel.pageOffset = 300;
        panel.refresh();
        var oldSummary = panel.summaryRequestId;
        var oldSessions = panel.sessionsRequestId;
        database.sessionsReady(oldSessions, []);
        database.summaryReady(oldSummary, {sessionCount: data.total});
        compare(panel.pageOffset, 300);
        database.dailyReady(panel.dailyRequestId, []);
        compare(panel.pageOffset, data.offset);
        verify(panel.summaryRequestId !== oldSummary);
        database.summaryReady(oldSummary, {sessionCount: 999});
        database.sessionsReady(oldSessions, [{sessionId: "stale"}]);
        verify(!panel.summaryCurrent);
        compare(panel.sessionRows.length, 0);
        finish(data.total, data.total ? [{sessionId: "retained"}] : []);
        compare(panel.pageOffset, data.offset);
        verify(!panel.loading);
        verify(!panel.hasNextPage);
    }
    function test_virtualized_session_focus_and_scroll_restored() {
        panel.selectedSessionId = "session-75";
        var rows = [];
        for (var i = 0; i < 100; ++i) rows.push({sessionId: "session-" + i, eventCount: 1});
        finish(100, rows);
        var list = findChild(panel, "sessionHistoryList");
        tryVerify(function() {
            var row = list.itemAtIndex(75);
            return row !== null && row.activeFocus;
        });
        verify(list.contentY > 0);
        compare(panel.selectedSessionId, "session-75");
    }
    function test_export_freezes_selection_and_keeps_independent_status() {
        panel.selectedProjectId = "chosen-project";
        panel.selectedModelFilter = {modelId: "chosen-model"};
        verify(panel.prepareExport("csv"));
        var frozen = panel.pendingExportSelection;
        verify(panel.exportBusy);
        verify(!panel.prepareExport("json"));
        panel.selectedToolId = "claude-code";
        panel.selectedRangeIndex = 2;
        database.importFinished("claude-code", 1, false, "");
        verify(panel.beginExport("/unused"));
        compare(database.exportCount, 1);
        compare(database.exportedSelection.tool, "codex-cli");
        compare(database.exportedSelection.project, "chosen-project");
        compare(database.exportedSelection.filter.modelId, "chosen-model");
        compare(database.exportedSelection.from, frozen.from);
        compare(database.exportedSelection.to, frozen.to);
        compare(database.exportedSelection.format, "csv");
        verify(!panel.beginExport("/other"));
        compare(database.exportCount, 1);
        var request = panel.exportRequestId;
        panel.refresh();
        compare(panel.exportState, "writing");
        database.exportReady("old-request", "session-export-write-failed");
        compare(panel.exportState, "writing");
        database.exportReady(request, "");
        compare(panel.exportState, "saved");
        verify(!panel.exportBusy);
        var message = panel.statusText;
        panel.refresh();
        compare(panel.statusText, message);
    }
    function test_export_errors_are_localized_and_not_cleared_by_refresh_data() {
        return ["session-export-format-invalid", "session-invalid-query", "session-database-unavailable",
                "session-export-destination-invalid", "session-export-write-failed", "session-query-failed", "future-code"]
            .map(function(key) { return {tag: key, key: key}; });
    }
    function test_export_errors_are_localized_and_not_cleared_by_refresh(data) {
        verify(panel.beginExport("/unused"));
        database.exportReady(panel.exportRequestId, data.key);
        compare(panel.exportState, "error");
        verify(panel.statusText.length > 0);
        verify(panel.statusText.indexOf(data.key) < 0);
        var message = panel.statusText;
        panel.refresh();
        compare(panel.statusText, message);
        verify(!panel.exportBusy);
    }
    SignalSpy { id: navigation; signalName: "sessionHistoryRequested" }
    function test_drilldown_preserves_source_filters() {
        navigation.target = panel;
        navigation.clear();
        panel.selectedProjectId = "original";
        panel.selectedModelFilter = {modelId: "old-model"};
        panel.drillDown("new-project", {modelId: "new-model"});
        compare(navigation.count, 1);
        compare(navigation.signalArguments[0][1], "new-project");
        compare(navigation.signalArguments[0][3], "");
        compare(navigation.signalArguments[0][4], 0);
        compare(navigation.signalArguments[0][5].modelId, "new-model");
        compare(panel.selectedProjectId, "original");
        compare(panel.selectedModelFilter.modelId, "old-model");
    }
}
