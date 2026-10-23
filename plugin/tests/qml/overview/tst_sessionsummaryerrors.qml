import QtQuick
import QtTest
import "../../../../package/contents/ui/views" as Views

TestCase {
    id: tests
    name: "OverviewSessionSummaryErrors"
    width: 1000
    height: 800
    visible: true
    when: windowShown
    QtObject {
        id: backend
        property bool codexEnabled: true
        property bool claudeCodeEnabled: true
        signal summaryReady(string requestId, var summary)
        signal queryFailed(string requestId, string errorKey)
        function requestSummary(id, tool, from, to, project) { }
    }
    QtObject {
        id: fakeMonitor
        property var localSessions: backend
        property var presentationDailyState: null
        property var guardrails: null
        property string modelMigrationNotice: ""
    }
    Component { id: viewFactory; Views.OverviewView { monitor: fakeMonitor; width: 950; height: 750 } }
    function test_recreated_view_rejects_previous_instance_replies() {
        var previous = createTemporaryObject(viewFactory, tests);
        verify(previous !== null);
        previous.refreshSessionSummaries();
        var previousCodex = previous.sessionSummaryLatestByTool["codex-cli"];
        var previousClaude = previous.sessionSummaryLatestByTool["claude-code"];
        previous.destroy();
        var current = createTemporaryObject(viewFactory, tests);
        verify(current !== null);
        current.refreshSessionSummaries();
        verify(current.sessionSummaryLatestByTool["codex-cli"] !== previousCodex);
        verify(current.sessionSummaryLatestByTool["claude-code"] !== previousClaude);
        backend.summaryReady(previousCodex, {sessionCount: 999});
        backend.queryFailed(previousClaude, "session-query-failed");
        compare(Object.keys(current.sessionSummaryByTool).length, 0);
        compare(Object.keys(current.sessionSummaryRequests).length, 2);
        backend.summaryReady(current.sessionSummaryLatestByTool["codex-cli"], {sessionCount: 4});
        backend.summaryReady(current.sessionSummaryLatestByTool["claude-code"], {sessionCount: 2});
        compare(current.sessionSummaryByTool["codex-cli"].sessionCount, 4);
        compare(current.sessionSummaryByTool["claude-code"].sessionCount, 2);
        compare(Object.keys(current.sessionSummaryRequests).length, 0);
    }
    Component {
        id: dailyStateFactory
        QtObject {
            property var rows: []
            function prioritizedSourceIds() { return rows.map(function(row) { return row.stableId; }); }
            function source(id) { return rows.find(function(row) { return row.stableId === id; }); }
        }
    }
    function test_search_miss_does_not_claim_all_sources_are_healthy_and_reset_recovers() {
        var view = createTemporaryObject(viewFactory, tests);
        verify(view !== null);
        var state = findChild(view, "overviewState");
        verify(state !== null);
        state.dailyState = createTemporaryObject(dailyStateFactory, tests, {
            rows: [{stableId: "failing", displayName: "Needs repair",
                attentionSeverity: "warning", nextActionText: "Check permissions"}]
        });
        view.sourceFilterIndex = 1;
        view.sourceSearchText = "missing";
        compare(view.visibleSourceRows.length, 0);
        var empty = findChild(view, "overviewSourcesEmptyState");
        var reset = findChild(view, "overviewClearSourceFilters");
        verify(empty.visible);
        compare(empty.text, "No source matches your search and current filter.");
        verify(reset.visible);
        reset.clicked();
        compare(view.sourceSearchText, "");
        compare(view.sourceFilterIndex, 0);
        compare(view.visibleSourceRows.length, 1);
        verify(!empty.visible);
        verify(!reset.visible);
        state.dailyState = createTemporaryObject(dailyStateFactory, tests, {
            rows: [{stableId: "healthy", displayName: "Healthy", attentionSeverity: "none"}]
        });
        view.sourceFilterIndex = 1;
        verify(empty.visible);
        compare(empty.text, "No sources currently need attention.");
        reset.clicked();
        compare(view.visibleSourceRows.length, 1);
    }
    function test_reset_available_with_results_resets_typed_controls_and_focus() {
        var view = createTemporaryObject(viewFactory, tests);
        var state = findChild(view, "overviewState");
        state.dailyState = createTemporaryObject(dailyStateFactory, tests, {
            rows: [{stableId: "failing", displayName: "Needs repair", attentionSeverity: "warning"},
                {stableId: "healthy", displayName: "Healthy", attentionSeverity: "none"}]
        });
        var search = findChild(view, "overviewSourceSearch");
        var filter = findChild(view, "overviewSourceFilter");
        var reset = findChild(view, "overviewClearSourceFilters");
        search.forceActiveFocus(Qt.TabFocusReason);
        tryCompare(search, "activeFocus", true);
        for (var letter of "repair") keyClick(letter);
        compare(search.text, "repair");
        compare(view.sourceSearchText, "repair");
        filter.incrementCurrentIndex();
        filter.activated(1);
        compare(view.sourceFilterIndex, 1);
        compare(view.visibleSourceRows.length, 1);
        verify(reset.visible);
        reset.forceActiveFocus(Qt.TabFocusReason);
        keyClick(Qt.Key_Space);
        compare(view.sourceSearchText, "");
        compare(view.sourceFilterIndex, 0);
        compare(search.text, "");
        compare(filter.currentIndex, 0);
        compare(view.visibleSourceRows.length, 2);
        verify(!reset.visible);
        tryCompare(search, "activeFocus", true);
    }
    function test_failure_completes_current_source_and_retry_replaces_it() {
        var view = createTemporaryObject(viewFactory, tests);
        verify(view !== null);
        view.refreshSessionSummaries();
        var old = view.sessionSummaryLatestByTool["codex-cli"];
        backend.summaryReady(old, {sessionCount: 8});
        verify(view.sessionActivityText("codex-cli").indexOf("8") >= 0);
        view.refreshSessionSummaries();
        var current = view.sessionSummaryLatestByTool["codex-cli"];
        backend.queryFailed(old, "session-query-failed");
        verify(!view.sessionSummaryByTool["codex-cli"].queryFailed);
        backend.queryFailed(current, "session-database-unavailable");
        verify(view.sessionSummaryByTool["codex-cli"].queryFailed);
        verify(view.sessionActivityText("codex-cli").indexOf("unavailable") >= 0);
        verify(view.sessionSummaryRequests[current] === undefined);
        backend.summaryReady(view.sessionSummaryLatestByTool["claude-code"], {sessionCount: 3});
        compare(Object.keys(view.sessionSummaryRequests).length, 0);
        for (var i = 0; i < 10; ++i) view.refreshSessionSummaries();
        compare(Object.keys(view.sessionSummaryRequests).length, 2);
        backend.summaryReady(view.sessionSummaryLatestByTool["codex-cli"], {sessionCount: 0});
        verify(!view.sessionSummaryByTool["codex-cli"].queryFailed);
        verify(view.sessionActivityText("codex-cli").indexOf("0") >= 0);
    }
}
