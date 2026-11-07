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
        property var sessionActivity: null
        property var favoriteSourceIds: []
        property var presentationDailyState: null
        property var guardrails: null
        property string modelMigrationNotice: ""
    }
    Component { id: viewFactory; Views.OverviewView { monitor: fakeMonitor; width: 950; height: 750 } }
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
    function test_favorite_filter_state_survives_restore() {
        var view = createTemporaryObject(viewFactory, tests);
        var state = findChild(view, "overviewState");
        state.favoriteSourceIds = ["healthy", "healthy", "disabled"];
        state.dailyState = createTemporaryObject(dailyStateFactory, tests, {
            rows: [{stableId: "failing", displayName: "Needs repair", attentionSeverity: "warning"},
                {stableId: "healthy", displayName: "Healthy", attentionSeverity: "none"}]
        });
        view.sourceFilterIndex = 2;
        compare(view.visibleSourceRows.length, 1);
        compare(view.visibleSourceRows[0].stableId, "healthy");
        var saved = view.captureViewState();
        view.sourceFilterIndex = 0;
        view.viewState = saved;
        compare(view.sourceFilterIndex, 2);
        compare(view.visibleSourceRows.length, 1);
        state.favoriteSourceIds = [];
        compare(view.visibleSourceRows.length, 0);
        verify(!view.restoreSourceFocus("healthy"));
        tryCompare(findChild(view, "overviewSourceSearch"), "activeFocus", true);
    }
}
