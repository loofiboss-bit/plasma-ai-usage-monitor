import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: tests
    name: "SharedSessionActivity"
    Component { id: backendFactory; QtObject {
        property bool codexEnabled: true
        property bool claudeCodeEnabled: false
        property var requests: []
        signal activitySummaryReady(string requestId, var summary)
        signal queryFailed(string requestId, string errorKey)
        signal importFinished()
        signal clearFinished(string requestId, bool ok, string error)
        function requestActivitySummary(id, tool, from, to) {
            requests = requests.concat([{id: id, tool: tool, from: from, to: to}]);
        }
    } }
    Component { id: stateFactory; Components.SessionActivityState {} }
    function setup() {
        var backend = createTemporaryObject(backendFactory, tests);
        var state = createTemporaryObject(stateFactory, tests, {
            sessionModel: backend, presentationTime: new Date(2026, 9, 4, 14, 30)
        });
        verify(state !== null);
        return {backend: backend, state: state};
    }
    function lastRequest(backend) { return backend.requests[backend.requests.length - 1]; }
    function test_shared_cached_zero_and_missing_tokens_are_distinct() {
        var pair = setup();
        pair.backend.activitySummaryReady(lastRequest(pair.backend).id,
            {sessionCount: 0, totalTokens: null, coverage: "unavailable"});
        var totals = pair.state.scopedTotals(null);
        verify(!totals.available);
        verify(pair.state.activityText("codex-cli").indexOf("No recorded") >= 0);
        verify(!pair.state.scopedTotals(["unavailable-favorite"]).available);
        pair.state.refresh();
        pair.backend.activitySummaryReady(lastRequest(pair.backend).id,
            {sessionCount: 1, eventCount: 1, totalTokens: 0, coverage: "reported"});
        compare(pair.state.scopedTotals(null).totalTokens, 0);
        verify(!pair.state.scopedTotals(null).partial);
    }
    function test_disable_clear_rollover_reject_old_replies() {
        var pair = setup();
        var old = lastRequest(pair.backend).id;
        pair.backend.codexEnabled = false;
        pair.backend.activitySummaryReady(old, {sessionCount: 99});
        compare(Object.keys(pair.state.summaryByTool).length, 0);
        pair.backend.codexEnabled = true;
        old = lastRequest(pair.backend).id;
        pair.backend.clearFinished("clear", true, "");
        pair.backend.activitySummaryReady(old, {sessionCount: 99});
        compare(Object.keys(pair.state.summaryByTool).length, 0);
        old = lastRequest(pair.backend).id;
        pair.state.presentationTime = new Date(2026, 9, 5, 0, 1);
        var fresh = lastRequest(pair.backend);
        verify(fresh.id !== old);
        compare(new Date(fresh.from).getHours(), 0);
        pair.backend.activitySummaryReady(old, {sessionCount: 99});
        compare(Object.keys(pair.state.summaryByTool).length, 0);
        pair.backend.queryFailed(fresh.id, "failed");
        verify(pair.state.summaryByTool["codex-cli"].queryFailed);
        pair.backend.importFinished();
        compare(Object.keys(pair.state.summaryByTool).length, 0);
    }
    function test_failure_retries_on_existing_clock_and_late_failure_is_ignored() {
        var pair = setup();
        var old = lastRequest(pair.backend).id;
        pair.state.refresh();
        var current = lastRequest(pair.backend).id;
        pair.backend.queryFailed(old, "old");
        compare(Object.keys(pair.state.summaryByTool).length, 0);
        pair.backend.queryFailed(current, "failed");
        verify(pair.state.summaryByTool["codex-cli"].queryFailed);
        pair.state.presentationTime = new Date(2026, 9, 4, 14, 31);
        verify(lastRequest(pair.backend).id !== current);
        pair.backend.activitySummaryReady(lastRequest(pair.backend).id,
            {sessionCount: 1, eventCount: 1, totalTokens: null, coverage: "partial"});
        var totals = pair.state.scopedTotals(null);
        verify(totals.available);
        compare(totals.totalTokens, null);
        verify(totals.partial);
        for (var i = 0; i < 10; ++i) pair.state.refresh();
        compare(Object.keys(pair.state.pending).length, 1);
    }
    Component { id: metricFactory; Components.CompactMetricState {} }
    function test_panel_activity_modes_global_warning_and_missing_values() {
        var metric = createTemporaryObject(metricFactory, tests, {
            summary: {enabledSourceCount: 0},
            globalSummary: {highestSeverity: "critical", mostUrgentSource: {stableId: "outside", displayName: "Outside favorite"}},
            activityTotals: {available: false}
        });
        compare(metric.statusKey(), "critical");
        compare(metric.normalizeMode("count"), "active-sources");
        compare(metric.normalizeMode("cost"), "actual-spend");
        compare(metric.displayText("session-tokens"), "\u2014");
        metric.activityTotals = {available: true, sessionCount: 1, totalTokens: 0, partial: false};
        verify(metric.displayText("session-tokens").indexOf("0") >= 0);
        verify(metric.displayText("session-count").indexOf("1") >= 0);
        metric.activityTotals = {available: true, sessionCount: 1, totalTokens: null, partial: true};
        compare(metric.displayText("session-tokens"), "\u2014");
        verify(metric.displayText("session-count").indexOf("partial") >= 0);
        verify(metric.summaryText().indexOf("Outside favorite") >= 0);
    }
    function test_partial_total_category_labels_reported_aggregate_as_partial() {
        var pair = setup();
        // Two events: (input=10, output=2) + (input=missing, output=8).
        pair.backend.activitySummaryReady(lastRequest(pair.backend).id, {
            sessionCount: 1, eventCount: 2, inputTokens: 10, outputTokens: 10,
            totalTokens: 20, coverage: "reported",
            categoryCoverage: {totalTokens: {reportedEventCount: 1, missingEventCount: 1, coverage: "partial"}}
        });
        var totals = pair.state.scopedTotals(null);
        compare(totals.totalTokens, 20);
        verify(totals.partial);
        var metric = createTemporaryObject(metricFactory, tests, {activityTotals: totals});
        verify(metric.displayText("session-tokens").indexOf("partial") >= 0);
    }
}
