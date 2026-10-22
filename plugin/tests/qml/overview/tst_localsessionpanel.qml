import QtQuick
import QtTest
import "../../../../package/contents/ui/Utils.js" as Utils
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: tests
    name: "LocalSessionPanelRefresh"
    visible: true
    when: windowShown
    width: 1200
    height: 900

    QtObject {
        id: fakeModel
        property bool codexEnabled: true
        property bool claudeCodeEnabled: false
        property int requests: 0
        property int exportRequests: 0
        property string lastExportFormat: ""
        property string lastExportPath: ""
        signal importFinished(string toolId, int importedCount, bool partial, string errorKey)
        function requestProjects(requestId) { requests++; }
        function requestSummary(requestId, toolId, from, to, project) { requests++; }
        function requestSessions(requestId, toolId, from, to, project, offset, limit) { requests++; }
        function requestDaily(requestId, toolId, from, to, project, zone) { requests++; }
        function requestExport(requestId, toolId, from, to, project, format, path) {
            exportRequests++;
            lastExportFormat = format;
            lastExportPath = path;
        }
    }

    SignalSpy { id: historySpy; signalName: "sessionHistoryRequested" }

    Component {
        id: panelComponent
        Components.LocalSessionPanel {
            sessionModel: fakeModel
            width: 1200
        }
    }

    function test_compactTokensAndExactIntegerCounts() {
        compare(Utils.formatCount(45, Qt.locale("sv_SE")), "45");
        compare(Utils.formatNumber(1234000000, Qt.locale("sv_SE")), "1,2\u202fB");
        compare(Utils.formatNumber(1250, Qt.locale("en_US")), "1.3\u202fK");
        compare(Utils.formatNumber(null), "");
        compare(Utils.formatCount(undefined), "");
        compare(Utils.formatNumber(NaN), "");
        compare(Utils.formatCount(0), "0");
    }

    function test_dailyDateRemainsTheReportedLocalCalendarDay() {
        var panel = createTemporaryObject(panelComponent, tests);
        compare(panel.dayText("2026-10-12"),
                new Date(2026, 9, 12).toLocaleDateString(Qt.locale(), Locale.ShortFormat));
        compare(panel.dayText("2024-02-29"),
                new Date(2024, 1, 29).toLocaleDateString(Qt.locale(), Locale.ShortFormat));
        var invalid = [null, "", "invalid", "2026-02-29", "2026-04-31",
                       "2026-13-01", "2026-00-01", "2026-10-00", "2026-1-12",
                       "2026-10-12T00:00:00Z"];
        for (var i = 0; i < invalid.length; ++i)
            compare(panel.dayText(invalid[i]), "Date unavailable");
    }

    function test_sessionExportKeepsAbsoluteFileDialogPathForBothFormats() {
        var panel = createTemporaryObject(panelComponent, tests);
        verify(panel !== null);

        for (var format of ["json", "csv"]) {
            var expectedPath = "/tmp/AI Usage/coding-sessions." + format;
            var selectedUrl = Qt.resolvedUrl("file:///tmp/AI%20Usage/coding-sessions." + format);

            compare(panel.localPath(selectedUrl), expectedPath);
            compare(panel.localPath(String(selectedUrl)), expectedPath);

            panel.pendingExportFormat = format;
            panel.beginExport(panel.localPath(selectedUrl));
            compare(fakeModel.lastExportFormat, format);
            compare(fakeModel.lastExportPath, expectedPath);
        }

        compare(fakeModel.exportRequests, 2);
    }

    function test_summaryHidesTechnicalDetailsAndPreservesHistoryFilter() {
        var panel = createTemporaryObject(panelComponent, tests);
        verify(panel !== null);
        compare(panel.presentationMode, "summary");
        verify(!panel.technicalDetailsExpanded);
        verify(!findChild(panel, "sessionTechnicalDetails").visible);
        verify(!findChild(panel, "sessionHistoryList").visible);
        panel.selectedProjectId = "project-123";
        panel.selectedRangeIndex = 2;
        panel.pageOffset = 100;
        panel.selectedSessionId = "session-123";
        historySpy.target = panel;
        historySpy.clear();
        panel.openSessionHistory();
        compare(historySpy.count, 1);
        compare(historySpy.signalArguments[0][0], "codex-cli");
        compare(historySpy.signalArguments[0][1], "project-123");
        compare(historySpy.signalArguments[0][2], 30);
        compare(historySpy.signalArguments[0][3], "session-123");
        compare(historySpy.signalArguments[0][4], 100);
        historySpy.target = null;
    }

    function test_restoresSavedFilterStateBeforeHistoryNavigation() {
        var state = {toolId: "codex-cli", projectId: "private-project-id",
                     rangeIndex: 2, pageOffset: 100, sessionId: "previous-selection"};
        var panel = createTemporaryObject(panelComponent, tests, {sessionState: state});
        compare(panel.selectedProjectId, "private-project-id");
        compare(panel.rangeDays, 30);
        compare(panel.pageOffset, 100);
        compare(panel.selectedSessionId, "previous-selection");
        panel.presentationMode = "history";
        compare(panel.sessionState.projectId, "private-project-id");
        compare(panel.sessionState.pageOffset, 100);
        compare(panel.fullDateText(null), "Unavailable");
    }

    function test_presentationChangesDoNotResetFiltersOrFetchAgain() {
        var panel = createTemporaryObject(panelComponent, tests);
        panel.selectedProjectId = "project-123";
        panel.selectedRangeIndex = 2;
        var before = fakeModel.requests;
        panel.presentationMode = "history";
        verify(findChild(panel, "sessionHistoryList").visible);
        verify(!panel.showDaily);
        panel.historyDisplayIndex = 1;
        verify(panel.showDaily);
        verify(!findChild(panel, "sessionHistoryList").visible);
        panel.presentationMode = "analysis";
        compare(panel.breakdownIndex, 0);
        verify(panel.showDaily);
        panel.breakdownIndex = 1;
        verify(!panel.showDaily);
        compare(panel.selectedProjectId, "project-123");
        compare(panel.rangeDays, 30);
        compare(fakeModel.requests, before);
    }

    function test_technicalDetailsCanBeOpenedWithKeyboard() {
        var panel = createTemporaryObject(panelComponent, tests);
        var button = findChild(panel, "sessionTechnicalDetailsButton");
        verify(button !== null);
        button.forceActiveFocus(Qt.TabFocusReason);
        tryCompare(button, "activeFocus", true);
        keyClick(Qt.Key_Space);
        tryCompare(panel, "technicalDetailsExpanded", true);
        verify(findChild(panel, "sessionTechnicalDetails").visible);
        keyClick(Qt.Key_Space);
        tryCompare(panel, "technicalDetailsExpanded", false);
    }

    function test_toolPickerRemainsAvailableWhenCodexIsDisabled() {
        fakeModel.codexEnabled = false;
        fakeModel.claudeCodeEnabled = true;
        var panel = createTemporaryObject(panelComponent, tests);
        verify(panel !== null);
        panel.presentationMode = "history";
        var picker = findChild(panel, "sessionToolPicker");
        verify(picker !== null);
        verify(picker.visible);
        compare(panel.activeToolId, "codex-cli");
        compare(findChild(panel, "sessionHistoryList").visible, false);

        picker.currentIndex = 1;
        picker.activated(1);
        compare(panel.activeToolId, "claude-code");
        compare(panel.selectedToolEnabled(), true);
        verify(findChild(panel, "sessionHistoryList").visible);
        fakeModel.codexEnabled = true;
        fakeModel.claudeCodeEnabled = false;
    }

    function test_dailyModelAndProjectViewsHaveExplicitEmptyAndErrorStates() {
        fakeModel.codexEnabled = true;
        var panel = createTemporaryObject(panelComponent, tests);
        verify(panel !== null);
        panel.presentationMode = "analysis";
        panel.breakdownIndex = 0;
        panel.dailyRows = [];
        panel.loading = false;
        verify(findChild(panel, "sessionDailyEmptyState").visible);

        panel.breakdownIndex = 1;
        panel.summary = ({modelBreakdown: [], projectBreakdown: []});
        verify(findChild(panel, "sessionModelEmptyState").visible);
        verify(!findChild(panel, "sessionProjectEmptyState").visible);
        panel.breakdownIndex = 2;
        verify(findChild(panel, "sessionProjectEmptyState").visible);

        panel.queryErrorKey = "session-database-unavailable";
        verify(findChild(panel, "sessionQueryError").visible);
        verify(!findChild(panel, "sessionModelEmptyState").visible);
    }

    function test_importRefreshesVisiblePanelAndRejectsOldListReply() {
        var panel = createTemporaryObject(panelComponent, tests);
        verify(panel !== null);
        var before = fakeModel.requests;
        var oldRequest = panel.sessionsRequestId;
        fakeModel.importFinished("codex-cli", 1, false, "");
        verify(fakeModel.requests > before);
        verify(panel.sessionsRequestId !== oldRequest);
        panel.acceptSessionRows(oldRequest, [{sessionId: "stale"}]);
        compare(panel.sessionRows.length, 0);
        panel.acceptSessionRows(panel.sessionsRequestId, [{sessionId: "current"}]);
        compare(panel.sessionRows[0].sessionId, "current");
        panel.visible = false;
        before = fakeModel.requests;
        fakeModel.importFinished("codex-cli", 1, false, "");
        compare(fakeModel.requests, before);
        panel.visible = true;
        verify(fakeModel.requests > before);
    }
}
