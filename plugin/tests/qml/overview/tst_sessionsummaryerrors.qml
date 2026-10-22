import QtQuick
import QtTest
import "../../../../package/contents/ui/views" as Views

TestCase {
    id: tests
    name: "OverviewSessionSummaryErrors"
    width: 1000
    height: 800
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
