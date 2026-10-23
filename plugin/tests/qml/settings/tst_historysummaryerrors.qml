import QtQuick
import QtTest
import "../../../../package/contents/ui" as AppUi

TestCase {
    id: tests
    name: "HistorySettingsSummaryErrors"
    width: 1000
    height: 800
    when: windowShown
    QtObject {
        id: backend
        signal summaryReady(string requestId, var summary)
        signal queryFailed(string requestId, string errorKey)
        signal importFinished(string toolId, int count, bool partial, string errorKey)
        signal clearFinished(string requestId, bool ok, string errorKey)
        function setToolEnabled(tool, enabled) { }
        function requestSummary(id, tool, from, to, project) { }
        function requestProjects(id) { }
        function projectAliases() { return []; }
    }
    QtObject {
        id: storage
        signal maintenanceFinished(string requestId, var result)
        signal exportFinished(string requestId, var result)
        function init() { }
        function requestStorageStatus(id) { }
    }
    function test_error_replaces_scan_and_stale_error_is_ignored() {
        var factory = Qt.createComponent(Qt.resolvedUrl("../../../../package/contents/ui/configHistory.qml"));
        compare(factory.status, Component.Ready, factory.errorString());
        var page = createTemporaryObject(factory, tests, {sessionBackend: backend, historyDatabase: storage});
        verify(page !== null);
        page.refreshSessionSummaries();
        var old = page.sessionSummaryRequestIds["codex-cli"];
        backend.summaryReady(old, {toolId: "codex-cli", importStatus: "complete"});
        compare(page.sessionSummaries["codex-cli"].importStatus, "complete");
        page.refreshSessionSummaries();
        var current = page.sessionSummaryRequestIds["codex-cli"];
        backend.queryFailed(old, "session-query-failed");
        verify(!page.sessionSummaries["codex-cli"].queryFailed);
        backend.queryFailed(current, "session-database-unavailable");
        verify(page.sessionSummaries["codex-cli"].queryFailed);
        verify(page.sessionSummaryRequestIds["codex-cli"] === undefined);
        page.refreshSessionSummaries();
        backend.summaryReady(page.sessionSummaryRequestIds["codex-cli"], {toolId: "codex-cli", importStatus: "complete"});
        verify(!page.sessionSummaries["codex-cli"].queryFailed);
    }

    function test_scan_text_keeps_last_result_visible_while_running() {
        var factory = Qt.createComponent(Qt.resolvedUrl("../../../../package/contents/ui/configHistory.qml"));
        compare(factory.status, Component.Ready, factory.errorString());
        var page = createTemporaryObject(factory, tests, {sessionBackend: backend, historyDatabase: storage});
        verify(page !== null);
        var running = page.scanStateText({state: "running", importedEventCount: 4,
            lastCompletedAtUtc: "2026-10-23T07:00:00.000Z"});
        verify(running.indexOf("Scanning local session files") >= 0);
        verify(running.indexOf("Previous scan imported 4 events") >= 0);
        verify(running.indexOf("Last completed:") >= 0);
        var partial = page.scanStateText({state: "partial", importedEventCount: 2,
            reasonKeys: ["file-size-limit"]});
        verify(partial.indexOf("2 events imported") >= 0);
        verify(partial.indexOf("file exceeded") >= 0);
        var failed = page.scanStateText({state: "error", importedEventCount: 0,
            errorKey: "session-database-write-failed"});
        verify(failed.indexOf("0 events imported") >= 0);
        verify(failed.indexOf("database could not be updated") >= 0);
    }
}
