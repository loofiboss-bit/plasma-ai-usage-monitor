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
        function requestProjects(id) { requests.projects = id; }
        function requestSummary(id, tool, from, to, project, filter) { requests.summary = id; filters.summary = filter; }
        function requestSessions(id, tool, from, to, project, offset, limit, filter) { requests.sessions = id; filters.sessions = filter; }
        function requestDaily(id, tool, from, to, project, zone, filter) { requests.daily = id; filters.daily = filter; }
        function requestExport(id, tool, from, to, project, format, path, filter) { filters.export = filter; }
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
