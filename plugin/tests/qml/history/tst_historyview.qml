import QtQuick
import QtTest
import "../../../../package/contents/ui/views" as Views

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

    Component {
        id: historyViewComponent
        Views.HistoryView {
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
        verify(findChild(view, "historyCopyCsvButton").enabled);
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
        tryCompare(controls, "columns", 5);
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
}
