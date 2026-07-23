import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "Phase6DailyStateExtraction"

    function i18n(message) {
        var result = message;
        for (var i = 1; i < arguments.length; i++)
            result = result.replace("%" + i, arguments[i]);
        return result;
    }

    function i18nc(context, message) {
        var args = [message];
        for (var i = 2; i < arguments.length; i++) args.push(arguments[i]);
        return i18n.apply(null, args);
    }

    function i18np(singular, plural, count) {
        var args = [count === 1 ? singular : plural];
        for (var i = 2; i < arguments.length; i++) args.push(arguments[i]);
        if (args.length === 1) args.push(count);
        return i18n.apply(null, args);
    }

    QtObject {
        id: fakeDb

        property string analystRequestId: ""
        property string catalogRequestId: ""
        property string seriesRequestId: ""
        property var historySources: []
        property string historyMetric: ""
        property int historyBucketMinutes: 0

        signal analystReady(string requestId, var result)
        signal historyCatalogReady(string requestId, var sources)
        signal historySeriesReady(string requestId, var result)

        function requestAnalyst(requestId, from, to, currency) {
            analystRequestId = requestId;
        }

        function requestHistoryCatalog(requestId) {
            catalogRequestId = requestId;
        }

        function requestHistorySeries(requestId, sources, from, to, metric,
                                      bucketMinutes) {
            seriesRequestId = requestId;
            historySources = sources;
            historyMetric = metric;
            historyBucketMinutes = bucketMinutes;
        }
    }

    QtObject {
        id: fakeDailyState

        function source(stableId) {
            return stableId === "openai" ? { freshnessState: "stale" } : {};
        }
    }

    Components.MetricAvailabilityFormatter {
        id: formatter
    }

    Components.CompactMetricState {
        id: compactState
    }

    Components.AnalystState {
        id: analystState
        db: fakeDb
    }

    Components.HistoryController {
        id: historyController
        usageDb: fakeDb
        dailyState: fakeDailyState
    }

    SignalSpy {
        id: reportSpy
        target: analystState
        signalName: "reportReady"
    }

    function init() {
        fakeDb.analystRequestId = "";
        fakeDb.catalogRequestId = "";
        fakeDb.seriesRequestId = "";
        fakeDb.historySources = [];
        fakeDb.historyMetric = "";
        fakeDb.historyBucketMinutes = 0;

        compactState.summary = {};

        analystState.snapshot = {};
        analystState.loading = false;
        analystState.displayRequestId = "";
        analystState.reportRequestId = "";
        analystState.reportDays = 0;
        analystState.requestGeneration = 0;
        reportSpy.clear();

        historyController.configuredProviders = [];
        historyController.configuredTools = [];
        historyController.sourceState.storedCatalog = [];
        historyController.compareMode = false;
        historyController.rangeIndex = 1;
        historyController.selectedSourceId = "";
        historyController.selectedMetric = "";
        historyController.loading = false;
        historyController.requestGeneration = 0;
        historyController.catalogRequestId = "";
        historyController.seriesRequestId = "";
        historyController.activeMetric = "";
        historyController.errorKey = "";
        historyController.seriesData = [];
        historyController.querySources = [];
    }

    function test_metricAvailabilityPreservesZeroAndMissing() {
        var zero = formatter.kpi({
            kpis: { spend: { available: true, value: 0 } }
        }, "spend");
        verify(zero.available);
        compare(zero.value, 0);

        var missing = formatter.kpi({ kpis: {} }, "spend");
        verify(!missing.available);
        compare(formatter.unavailableValue(), "\u2014");
        compare(formatter.formatPercent(0), "0.0%");
    }

    function test_compactMetricSelectionIsIndependentOfRendering() {
        compactState.summary = { enabledSourceCount: 1 };
        compare(compactState.normalizeMode("count"), "active-sources");
        compare(compactState.normalizeMode("critical"), "attention");
        compare(compactState.normalizeMode("unsupported"), "icon");
        compare(compactState.displayText("lowest-quota"), "\u2014");
        compare(compactState.statusKey(), "unverified");

        compactState.summary = {
            enabledSourceCount: 1,
            reportingUsefulSourceCount: 1,
            remainingRequests: { stableId: "openai", value: 0 },
            lowestActualRemainingQuota: {
                stableId: "openai",
                displayName: "OpenAI",
                percentRemaining: 0
            }
        };
        compare(compactState.displayText("requests"), "0 req");
        compare(compactState.displayText("lowest-quota"), "0% · OpenAI");
        compare(compactState.statusKey(), "healthy");
    }

    function test_analystStateAcceptsFixturesAndIgnoresOlderResults() {
        analystState.refreshData();
        verify(analystState.loading);
        verify(fakeDb.analystRequestId.indexOf("analyst-display-") === 0);

        analystState.acceptResult("older-request", {
            ok: true,
            marker: "old"
        });
        verify(analystState.loading);
        compare(analystState.snapshot.marker, undefined);

        analystState.acceptResult(fakeDb.analystRequestId, {
            ok: true,
            marker: "current",
            spendSeries: [{
                date: "2026-07-01",
                actualAvailable: true,
                actual: 0,
                estimatedAvailable: false,
                estimated: null
            }],
            activitySeries: [{
                date: "2026-07-01",
                tokensAvailable: true,
                tokens: 0
            }]
        });
        verify(!analystState.loading);
        compare(analystState.snapshot.marker, "current");
        compare(analystState.spendChartSeries()[0].points[0].value, 0);
        verify(analystState.spendChartSeries()[0].points[0].available);
        verify(!analystState.spendChartSeries()[1].points[0].available);
        verify(analystState.seriesHasValues("tokens"));
    }

    function test_analystReportUsesItsRequestedPeriodFixture() {
        analystState.requestReport(7);
        var requestId = fakeDb.analystRequestId;
        verify(requestId.indexOf("analyst-report-7-") === 0);

        analystState.acceptResult(requestId, {
            ok: true,
            generatedAt: "2026-07-07T12:00:00Z",
            from: "2026-07-01T00:00:00Z",
            to: "2026-07-07T23:59:59Z",
            coverage: {
                observedDayCount: 7,
                requestedDayCount: 7,
                percent: 100
            },
            currencyStatus: "single",
            currency: "USD",
            actualSampleCount: 7,
            estimatedSampleCount: 0,
            kpis: {
                averageDailySpend: { available: true, value: 0 },
                weekOverWeekChange: {
                    available: false,
                    reasonKey: "incomplete_comparison_windows"
                },
                volatility: {
                    available: false,
                    reasonKey: "zero_baseline"
                },
                outputInputRatio: {
                    available: false,
                    reasonKey: "insufficient_ratio_samples",
                    sampleCount: 0,
                    minimumSamples: 3
                }
            },
            anomaliesAvailable: true,
            anomalies: []
        });

        compare(reportSpy.count, 1);
        verify(reportSpy.signalArguments[0][0].indexOf("(7 days)") >= 0);
        verify(reportSpy.signalArguments[0][0].indexOf("Coverage: 7 of 7") >= 0);
        verify(reportSpy.signalArguments[0][0].indexOf("Average daily spend (actual):") >= 0);
        compare(analystState.reportRequestId, "");
    }

    function test_historyControllerIsFixtureDrivenAndMarksStaleSources() {
        historyController.configuredProviders = [{
            configKey: "openai",
            dbName: "OpenAI",
            name: "OpenAI",
            enabled: false,
            color: "#10a37f"
        }];
        historyController.configuredTools = [{
            stableId: "codex-cli",
            name: "Codex CLI",
            enabled: false
        }];
        historyController.selectedSourceId = "provider:OpenAI";

        historyController.refreshCatalog();
        var catalogId = fakeDb.catalogRequestId;
        historyController.acceptCatalog(catalogId, [{
            historyId: "provider:OpenAI",
            dbName: "OpenAI",
            displayName: "OpenAI",
            sourceKind: "provider",
            metricKinds: ["cost"],
            sampleCount: 2
        }, {
            historyId: "tool:Codex CLI",
            dbName: "Codex CLI",
            displayName: "Codex CLI",
            sourceKind: "tool",
            metricKinds: ["usageCount"],
            sampleCount: 3
        }]);

        compare(historyController.sourceRows.length, 2);
        compare(historyController.selectedSourceId, "provider:OpenAI");
        compare(historyController.sourceIndex(), 1);
        compare(historyController.metricIndex(), 0);
        compare(fakeDb.historyMetric, "cost");
        compare(fakeDb.historySources.length, 1);
        verify(fakeDb.historySources[0].stale);
        compare(fakeDb.historyBucketMinutes, 60);

        historyController.acceptSeries("older-series", {
            ok: true,
            series: [{ marker: "old" }]
        });
        compare(historyController.seriesData.length, 0);

        historyController.acceptSeries(fakeDb.seriesRequestId, {
            ok: true,
            series: [{
                sourceId: "provider:OpenAI",
                dbName: "OpenAI",
                displayName: "OpenAI",
                sourceKind: "provider",
                metricKind: "cost",
                unit: "currency",
                currency: "USD",
                semantic: "interval_total",
                sampleCount: 2,
                availablePointCount: 1,
                containsGaps: true,
                stale: true,
                points: [
                    { timestamp: "2026-07-01T00:00:00Z",
                      available: true, value: 0 },
                    { timestamp: "2026-07-02T00:00:00Z",
                      available: false, value: null }
                ]
            }]
        });
        compare(historyController.seriesData.length, 1);
        compare(historyController.seriesData[0].color, "#10a37f");
        verify(historyController.seriesData[0].name.indexOf("USD") >= 0);
        verify(historyController.coverageText(
            historyController.seriesData[0]).indexOf("contains gaps") >= 0);

        var csv = historyController.exportPayload("csv");
        verify(csv.indexOf(",0,true") >= 0);
        verify(csv.indexOf(",,false") >= 0);
    }

    function test_historyControllerExposesCompatibilityFailure() {
        historyController.configuredProviders = [{
            configKey: "openai",
            dbName: "OpenAI",
            name: "OpenAI",
            enabled: true
        }];
        historyController.sourceState.storedCatalog = [{
            historyId: "provider:OpenAI",
            dbName: "OpenAI",
            displayName: "OpenAI",
            sourceKind: "provider",
            metricKinds: ["cost"],
            sampleCount: 2
        }];
        historyController.selectedSourceId = "provider:OpenAI";
        historyController.selectedMetric = "cost";
        historyController.refresh();

        historyController.acceptSeries(fakeDb.seriesRequestId, {
            ok: false,
            errorKey: "mixed_currencies"
        });
        compare(historyController.errorKey, "mixed_currencies");
        verify(historyController.errorText().indexOf("different currencies") >= 0);
        compare(historyController.seriesData.length, 0);
    }
}
