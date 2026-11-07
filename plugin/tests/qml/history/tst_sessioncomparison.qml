import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "SessionComparison"
    width: 420
    height: 900
    visible: true
    when: windowShown

    QtObject {
        id: backend
        property var requests: []
        signal comparisonReady(string requestId, var payload)
        signal queryFailed(string requestId, string errorKey)
        signal clearFinished(string requestId, bool ok, string errorKey)
        signal enabledChanged()
        function requestComparison(id, tool, from, to, project, zone, filter) {
            requests = requests.concat([{id: id, tool: tool, from: from, to: to,
                                        project: project, zone: zone, filter: filter}]);
        }
    }
    Component {
        id: comparisonComponent
        Components.SessionComparison {
            width: 400
            sessionModel: backend
            toolId: "codex-cli"
            fromUtc: "2026-10-18T22:00:00.000Z"
            toUtc: "2026-10-25T23:00:00.000Z"
        }
    }
    property var comparison: null
    function init() {
        backend.requests = [];
        comparison = createTemporaryObject(comparisonComponent, testCase);
        verify(comparison !== null);
        wait(0);
    }
    function test_filter_and_stale_generation() {
        compare(backend.requests.length, 0);
        comparison.projectId = "anonymous-project";
        comparison.modelFilter = {modelId: "exact-model"};
        comparison.active = true;
        tryVerify(function() { return backend.requests.length > 0; });
        var first = backend.requests[backend.requests.length - 1];
        compare(first.project, "anonymous-project");
        compare(first.filter.modelId, "exact-model");
        comparison.projectId = "another-project";
        tryVerify(function() { return comparison.requestId !== first.id; });
        backend.comparisonReady(first.id, {current: {totalTokens: 999}});
        compare(comparison.result.current, undefined);
        backend.comparisonReady(comparison.requestId, {
            current: {totalTokens: 12, fromUtc: first.from, toUtc: first.to},
            previous: {totalTokens: 0, fromUtc: "2026-10-11T22:00:00.000Z", toUtc: first.from},
            deltas: {totalTokens: {available: true, absolute: 12,
                                   percentAvailable: false, reason: "zero-baseline"}}
        });
        compare(comparison.loading, false);
        verify(comparison.deltaText(comparison.result.deltas.totalTokens).indexOf("zero") >= 0);
        verify(findChild(comparison, "sessionComparisonPeriods").text.indexOf("Invalid") < 0);
        verify(comparison.implicitWidth <= testCase.width);
    }
    function test_retry_disable_and_missing_categories() {
        comparison.active = true;
        tryVerify(function() { return comparison.requestId !== ""; });
        var failed = comparison.requestId;
        backend.queryFailed(failed, "session-query-failed");
        compare(comparison.loading, false);
        verify(comparison.errorKey !== "");
        comparison.refresh();
        verify(comparison.requestId !== failed);
        compare(comparison.errorKey, "");
        verify(comparison.deltaText({available: false, reason: "missing-category"}).indexOf("omit") >= 0);
        compare(comparison.numberText(null), "Unavailable");
        compare(comparison.numberText(0), "0");
        var pending = comparison.requestId;
        comparison.active = false;
        wait(0);
        backend.comparisonReady(pending, {current: {totalTokens: 42}});
        compare(comparison.result.current, undefined);
        compare(comparison.loading, false);
    }
}
