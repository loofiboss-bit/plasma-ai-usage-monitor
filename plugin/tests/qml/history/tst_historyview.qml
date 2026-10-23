import QtQuick
import QtTest
import "../../../../package/contents/ui/views" as Views
import "../../../../package/contents/ui" as MonitorUi

TestCase {
    id: testCase
    name: "HistoryView"
    width: 1000
    height: 700

    QtObject {
        id: fakeDb
        property bool enabled: true
        property int catalogRequestCount: 0
        property int seriesRequestCount: 0
        property string catalogRequestId: ""
        property string seriesRequestId: ""
        property string lastMetric: ""
        property int lastBucketMinutes: 0
        property var lastSources: []
        property var lastFrom: null
        property var lastTo: null
        property int exportRequestCount: 0
        property string lastExportFormat: ""
        property bool codexEnabled: true
        property bool claudeCodeEnabled: true
        property int retentionDays: 30
        property var sessionRanges: ({})

        signal observationsChanged()
        signal historyCatalogReady(string requestId, var sources)
        signal historySeriesReady(string requestId, var result)

        function requestHistoryCatalog(requestId) {
            catalogRequestCount++;
            catalogRequestId = requestId;
        }

        function requestHistorySeries(requestId, sources, from, to, metric,
                                      bucketMinutes) {
            seriesRequestCount++;
            seriesRequestId = requestId;
            lastSources = sources;
            lastFrom = from;
            lastTo = to;
            lastMetric = metric;
            lastBucketMinutes = bucketMinutes;
        }

        function exportSelectedSeries(series, format, metric, from, to,
                                      bucketMinutes) {
            exportRequestCount++;
            lastExportFormat = format;
            lastMetric = metric;
            lastFrom = from;
            lastTo = to;
            lastBucketMinutes = bucketMinutes;
            return "history-export-" + format;
        }

        function requestProjects(requestId) {}
        function requestSummary(requestId, toolId, from, to, projectId, filter) {
            sessionRanges.summary = {toolId: toolId, from: from, to: to,
                                     projectId: projectId, filter: filter};
        }
        function requestSessions(requestId, toolId, from, to, projectId,
                                 offset, limit, filter) {
            sessionRanges.sessions = {toolId: toolId, from: from, to: to,
                                      projectId: projectId, filter: filter};
        }
        function requestDaily(requestId, toolId, from, to, projectId, zone, filter) {
            sessionRanges.daily = {toolId: toolId, from: from, to: to,
                                   projectId: projectId, filter: filter};
        }
        function requestExport(requestId, toolId, from, to, projectId,
                               format, path, filter) {
            sessionRanges.export = {toolId: toolId, from: from, to: to,
                                    projectId: projectId, filter: filter};
        }
    }

    QtObject {
        id: fakeMonitor
        property var usageDb: fakeDb
        property var presentationDailyState: null
        property var allProviders: [{
            configKey: "openai",
            dbName: "OpenAI",
            name: "OpenAI",
            enabled: true,
            color: "#10a37f"
        }]
        property var allSubscriptionTools: [{
            stableId: "codex-cli",
            name: "Codex CLI",
            enabled: true,
            monitor: { toolColor: "#8064d9" }
        }]
        property var localSessions: null
    }

    QtObject {
        id: fullMonitor
        property var usageDb: fakeDb
        property var presentationDailyState: null
        property var allProviders: fakeMonitor.allProviders
        property var allSubscriptionTools: fakeMonitor.allSubscriptionTools
        property var localSessions: fakeDb
        function refreshAll() {}
    }

    Component {
        id: historyViewComponent
        Views.HistoryView {
            width: 1000
            height: 700
        }
    }

    Component {
        id: fullRepresentationComponent
        MonitorUi.FullRepresentation {
            width: 1000
            height: 700
        }
    }

    function init() {
        fakeDb.catalogRequestCount = 0;
        fakeDb.seriesRequestCount = 0;
        fakeDb.catalogRequestId = "";
        fakeDb.seriesRequestId = "";
        fakeDb.lastMetric = "";
        fakeDb.lastBucketMinutes = 0;
        fakeDb.lastSources = [];
        fakeDb.lastFrom = null;
        fakeDb.lastTo = null;
        fakeDb.exportRequestCount = 0;
        fakeDb.lastExportFormat = "";
        fakeDb.sessionRanges = ({});
    }

    function storedSources() {
        return [{
            historyId: "provider:OpenAI",
            dbName: "OpenAI",
            displayName: "OpenAI",
            sourceKind: "provider",
            metricKinds: ["cost", "tokens", "requests"],
            sampleCount: 3
        }, {
            historyId: "tool:Codex CLI",
            dbName: "Codex CLI",
            displayName: "Codex CLI",
            sourceKind: "tool",
            metricKinds: ["usageCount", "percentUsed", "remaining"],
            sampleCount: 5
        }];
    }

    function test_catalogLoadsAfterLoaderInjectsMonitorAndPercentageCanBeSelected() {
        var view = createTemporaryObject(historyViewComponent, testCase);
        verify(view !== null);
        compare(fakeDb.catalogRequestCount, 0);

        view.monitor = fakeMonitor;
        tryCompare(fakeDb, "catalogRequestCount", 1);
        findChild(view, "historyController").selectedSourceId = "tool:Codex CLI";
        verify(fakeDb.catalogRequestId.length > 0);
        fakeDb.historyCatalogReady(fakeDb.catalogRequestId, storedSources());
        tryCompare(fakeDb, "seriesRequestCount", 1);

        var metricCombo = findChild(view, "historyMetricCombo");
        verify(metricCombo !== null);
        compare(metricCombo.count, 3);
        compare(metricCombo.currentValue, "usageCount");

        metricCombo.currentIndex = 1;
        metricCombo.activated(1);
        compare(fakeDb.lastMetric, "percentUsed");
        compare(fakeDb.lastSources.length, 1);
        compare(fakeDb.lastSources[0].historyId, "tool:Codex CLI");

        var rangeCombo = findChild(view, "historyRangeCombo");
        rangeCombo.currentIndex = 3;
        rangeCombo.activated(3);
        compare(fakeDb.lastBucketMinutes, 720);
        verify(fakeDb.lastTo.getTime() - fakeDb.lastFrom.getTime()
               >= 89 * 86400000);

        var compareButton = findChild(view, "historyCompareMode");
        compareButton.click();
        tryVerify(function() { return compareButton.checked; });
        compare(fakeDb.lastMetric, "percentUsed");
        compare(fakeDb.lastSources.length, 1);
        compare(fakeDb.lastSources[0].historyId, "tool:Codex CLI");
        verify(!findChild(view, "historySourceCombo").visible);
    }

    function test_emptyCatalogExplainsMissingMetricsAndDisablesSelector() {
        var view = createTemporaryObject(historyViewComponent, testCase);
        verify(view !== null);
        view.monitor = fakeMonitor;
        tryCompare(fakeDb, "catalogRequestCount", 1);
        fakeDb.historyCatalogReady(fakeDb.catalogRequestId, [{
            historyId: "tool:Codex CLI",
            dbName: "Codex CLI",
            displayName: "Codex CLI",
            sourceKind: "tool",
            metricKinds: [],
            sampleCount: 0
        }]);

        var metricCombo = findChild(view, "historyMetricCombo");
        compare(metricCombo.count, 0);
        verify(!metricCombo.enabled);
        compare(findChild(view, "historyEmptyState").title,
                "No usage metrics have been retained");
        verify(findChild(view, "historyEmptyState").details.indexOf(
            "no saved usage snapshots") >= 0);
        var controller = findChild(view, "historyController");
        compare(controller.catalogLoading, false);
        compare(controller.loading, false);
        compare(controller.seriesData.length, 0);
    }

    function test_observationChangesRefreshCatalogAndUsageResults() {
        var view = createTemporaryObject(historyViewComponent, testCase);
        verify(view !== null);
        view.monitor = fakeMonitor;
        tryCompare(fakeDb, "catalogRequestCount", 1);
        findChild(view, "historyController").selectedSourceId = "tool:Codex CLI";
        fakeDb.historyCatalogReady(fakeDb.catalogRequestId, storedSources());
        tryCompare(fakeDb, "seriesRequestCount", 1);

        fakeDb.historySeriesReady(fakeDb.seriesRequestId, {
            ok: true,
            series: [{
                sourceId: "tool:Codex CLI",
                dbName: "Codex CLI",
                displayName: "Codex CLI",
                sourceKind: "tool",
                metricKind: "usageCount",
                unit: "count",
                semantic: "rolling_gauge",
                sampleCount: 2,
                availablePointCount: 2,
                containsGaps: false,
                points: [{
                    timestamp: "2026-10-12T00:00:00Z",
                    value: 12,
                    available: true
                }]
            }]
        });

        var chart = findChild(view, "historyUsageChart");
        compare(findChild(view, "historyController").seriesData.length, 1);
        compare(chart.seriesData.length, 1);
        verify(findChild(view, "historyExportButton").enabled);
        verify(findChild(view, "historyCopyCsvMenuItem") !== null);
        verify(findChild(view, "historyExportMenu") !== null);
        compare(findChild(view, "historySelectionContext").text,
                "Codex CLI · Tool usage · Last 7 days");
        var controller = findChild(view, "historyController");
        compare(controller.exportPayload("csv"), "history-export-csv");
        compare(controller.exportPayload("json"), "history-export-json");
        compare(fakeDb.exportRequestCount, 2);

        var previousCatalogRequests = fakeDb.catalogRequestCount;
        fakeDb.observationsChanged();
        tryCompare(fakeDb, "catalogRequestCount", previousCatalogRequests + 1,
                   1500);
    }

    function test_sessionsModeResetsScrollAndUsesTheAvailableWidth() {
        var view = createTemporaryObject(historyViewComponent, testCase, {
            width: 420,
            height: 620
        });
        verify(view !== null);
        var controls = findChild(view, "historyControls");
        var scroll = findChild(view, "historyContentScroll");
        var content = findChild(view, "historyContent");
        compare(controls.columns, 1);

        view.width = 1200;
        tryCompare(controls, "columns", 4);
        compare(scroll.contentWidth, scroll.availableWidth);

        content.height = scroll.availableHeight + 300;
        var flickable = scroll.contentItem;
        flickable.contentY = 200;
        verify(flickable.contentY > 0);
        view.sessionsMode = true;
        tryCompare(flickable, "contentY", 0);

        compare(view.sessionsMode, true);
        var sessionsPanel = findChild(view, "historySessionsPanel");
        verify(sessionsPanel !== null);
        compare(sessionsPanel.presentationMode, "history");
    }

    function test_viewStateCapturesAndRestoresUsageAndSessionFilters() {
        var state = {
            sessionsMode: false,
            selectedSourceId: "tool:Codex CLI",
            selectedMetric: "usageCount",
            rangeIndex: 3,
            compareMode: true,
            contentY: 140,
            sessionToolId: "claude-code",
            sessionProjectId: "project-42",
            session: {
                toolId: "claude-code",
                projectId: "project-42",
                rangeIndex: 2,
                pageOffset: 100,
                sessionId: "session-42",
                modelSearch: "sonnet",
                historyDisplayIndex: 1
            }
        };
        var view = createTemporaryObject(historyViewComponent, testCase);
        verify(view !== null);
        view.viewState = state;
        verify(!view.applyViewState(state));
        view.monitor = fakeMonitor;
        tryCompare(fakeDb, "catalogRequestCount", 1);
        verify(!view.applyViewState(state));
        fakeDb.historyCatalogReady(fakeDb.catalogRequestId, storedSources());
        tryVerify(function() { return fakeDb.seriesRequestCount > 0; });
        var scrollArea = findChild(view, "historyContentScroll");
        verify(scrollArea !== null);
        var flickable = scrollArea.contentItem;
        verify(view.restoreScrollPending);
        var historyContent = findChild(view, "historyContent");
        verify(historyContent !== null);
        var spacer = Qt.createQmlObject(
            "import QtQuick; Item { implicitWidth: 1; implicitHeight: 800 }",
            historyContent);
        verify(spacer !== null);
        tryCompare(flickable, "contentY", 140);

        compare(view.sessionsMode, false);
        compare(findChild(view, "historyController").selectedSourceId,
                "tool:Codex CLI");
        compare(findChild(view, "historyController").selectedMetric,
                "usageCount");
        compare(findChild(view, "historyController").rangeIndex, 3);
        compare(findChild(view, "historyController").compareMode, true);
        compare(view.sessionToolId, "claude-code");
        compare(view.sessionProjectId, "project-42");
        compare(view.sessionState.rangeIndex, 2);
        compare(view.sessionState.pageOffset, 100);
        compare(view.sessionState.sessionId, "session-42");
        compare(view.sessionState.modelSearch, "sonnet");
        compare(view.sessionState.historyDisplayIndex, 1);
        flickable.contentY = 170;
        compare(view.captureViewState().contentY, 170);
        compare(findChild(view, "historySessionsPanel").selectedToolId,
                "claude-code");
        compare(findChild(view, "historySessionsPanel").selectedProjectId,
                "project-42");
        compare(findChild(view, "historySessionsPanel").pageOffset, 100);

        view.sessionsMode = true;
        var captured = view.captureViewState();
        compare(captured.sessionsMode, true);
        compare(captured.sessionToolId, "claude-code");
        compare(captured.session.projectId, "project-42");
        compare(captured.session.pageOffset, 100);
        compare(captured.session.modelSearch, "sonnet");
        compare(captured.session.historyDisplayIndex, 1);
        compare(captured.selectedMetric, "usageCount");
        compare(captured.compareMode, true);
    }

    function test_emptyViewStateDoesNotResetSessionHistoryHandoff() {
        var view = createTemporaryObject(historyViewComponent, testCase);
        verify(view !== null);

        view.sessionsMode = true;
        view.sessionToolId = "claude-code";
        view.sessionProjectId = "project-42";
        view.sessionState = ({
            toolId: "claude-code",
            projectId: "project-42",
            rangeIndex: 2,
            pageOffset: 80,
            sessionId: "session-42"
        });
        view.viewState = ({});

        wait(0);
        compare(view.sessionsMode, true);
        compare(view.sessionToolId, "claude-code");
        compare(view.sessionProjectId, "project-42");
        compare(view.sessionState.rangeIndex, 2);
        compare(view.sessionState.pageOffset, 80);
        compare(view.sessionState.sessionId, "session-42");

        var sessionsPanel = findChild(view, "historySessionsPanel");
        verify(sessionsPanel !== null);
        compare(sessionsPanel.selectedToolId, "claude-code");
        compare(sessionsPanel.selectedProjectId, "project-42");
        compare(sessionsPanel.selectedRangeIndex, 2);
        compare(sessionsPanel.pageOffset, 80);
        compare(sessionsPanel.selectedSessionId, "session-42");
    }

    function test_fullRepresentationRoutesDayAndRestoresHistoryStateInLoadedView() {
        var originalSessionState = {
            toolId: "claude-code",
            projectId: "project-42",
            modelFilter: {modelId: "sonnet"},
            rangeIndex: 3,
            rangeMode: "custom",
            customStartDate: "2026-10-01",
            customEndDate: "2026-10-20",
            historyDisplayIndex: 1,
            dayDrilldown: false,
            dailyScrollY: 80
        };
        var full = createTemporaryObject(fullRepresentationComponent, testCase, {
            monitor: fullMonitor,
            destination: 1,
            historySessionsMode: true,
            historySessionToolId: "claude-code",
            historySessionProjectId: "project-42",
            localSessionViewState: {
                sourceDetail: ({}), history: originalSessionState,
                analyst: ({})
            }
        });
        verify(full !== null);
        var loader = findChild(full, "destinationLoader");
        verify(loader !== null);
        tryCompare(loader, "status", Loader.Ready);
        var view = loader.item;
        verify(view !== null);
        compare(view.sessionsMode, true);
        var panel = findChild(view, "historySessionsPanel");
        verify(panel !== null);
        compare(panel.selectedToolId, "claude-code");
        compare(panel.selectedProjectId, "project-42");
        compare(panel.selectedModelFilter.modelId, "sonnet");
        compare(panel.selectedRangeIndex, 3);

        view.sessionHistoryRangeRequested({
            toolId: "claude-code", projectId: "project-42",
            modelFilter: {modelId: "sonnet"},
            startDate: "2026-10-20", endDate: "2026-10-20",
            dayDrilldown: true
        });
        tryCompare(panel, "selectedRangeIndex", 3);
        compare(loader.item, view);
        tryCompare(panel, "customStartDate", "2026-10-20");
        compare(panel.customEndDate, "2026-10-20");
        compare(panel.selectedToolId, "claude-code");
        compare(panel.selectedProjectId, "project-42");
        compare(panel.selectedModelFilter.modelId, "sonnet");
        verify(panel.sessionState.dayDrilldown);

        panel.refresh();
        var expectedRange = panel.localRange();
        compare(fakeDb.sessionRanges.summary.from, expectedRange.from);
        compare(fakeDb.sessionRanges.summary.to, expectedRange.to);
        compare(fakeDb.sessionRanges.sessions.from, expectedRange.from);
        compare(fakeDb.sessionRanges.daily.from, expectedRange.from);

        var returnButton = findChild(panel, "sessionDayDrilldownReturn");
        verify(returnButton !== null);
        returnButton.clicked();
        tryCompare(panel, "customStartDate", "2026-10-01");
        compare(loader.item, view);
        compare(panel.customEndDate, "2026-10-20");
        compare(panel.selectedToolId, "claude-code");
        compare(panel.selectedProjectId, "project-42");
        compare(panel.selectedModelFilter.modelId, "sonnet");
        compare(panel.sessionState.dayDrilldown, false);
        compare(panel.sessionState.dailyScrollY, 80);
    }

    function test_fullRepresentationPresetHandoffClearsCustomAndDayState() {
        var full = createTemporaryObject(fullRepresentationComponent, testCase, {
            monitor: fullMonitor,
            destination: 1,
            historySessionsMode: true,
            localSessionViewState: {
                sourceDetail: ({}),
                history: {
                    toolId: "claude-code", projectId: "project-42",
                    rangeIndex: 3, rangeMode: "custom",
                    customStartDate: "2026-10-01", customEndDate: "2026-10-20",
                    dayDrilldown: true, dayFocusDate: "2026-10-20",
                    dailyScrollY: 100
                },
                analyst: ({})
            }
        });
        verify(full !== null);
        var loader = findChild(full, "destinationLoader");
        verify(loader !== null);
        tryCompare(loader, "status", Loader.Ready);

        full.destination = 2;
        tryCompare(loader, "status", Loader.Ready);
        tryVerify(function() { return loader.item && typeof loader.item.captureViewState === "function"; });
        full.routeToSessionHistory("codex-cli", "project-new", 7,
                                   "session-42", 100, {modelId: "gpt-5-codex"});
        tryVerify(function() {
            return loader.item && typeof loader.item.applyViewState === "function"
                && loader.item.sessionsMode === true;
        });
        var panel = findChild(loader.item, "historySessionsPanel");
        verify(panel !== null);
        compare(panel.selectedToolId, "codex-cli");
        compare(panel.selectedProjectId, "project-new");
        compare(panel.selectedRangeIndex, 1);
        compare(panel.sessionState.rangeMode, "preset");
        compare(panel.customStartDate, "");
        compare(panel.customEndDate, "");
        compare(panel.sessionState.dayDrilldown, false);
        compare(full.sessionDayReturnState, null);
        compare(panel.selectedSessionId, "session-42");
        compare(panel.pageOffset, 100);
        compare(panel.selectedModelFilter.modelId, "gpt-5-codex");
    }
}
