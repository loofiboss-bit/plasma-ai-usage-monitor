import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/components" as Components
import "../../../../package/contents/ui/Utils.js" as Utils

TestCase {
    id: testCase
    name: "HistoryState"

    Components.HistoryState {
        id: state
    }

    Component {
        id: chartComponent
        Monitor.MultiSeriesChart {
            width: 500
            height: 300
        }
    }

    function init() {
        state.configuredProviders = [];
        state.configuredTools = [];
        state.storedCatalog = [];
    }

    function test_disabledConfiguredSourcesKeepRetainedHistory() {
        state.configuredProviders = [{
            configKey: "openai", dbName: "OpenAI", name: "OpenAI",
            enabled: false, color: "#10a37f"
        }];
        state.configuredTools = [{
            stableId: "codex-cli", name: "Codex CLI", enabled: false
        }];
        state.storedCatalog = [{
            historyId: "provider:OpenAI", dbName: "OpenAI",
            displayName: "OpenAI", sourceKind: "provider",
            metricKinds: ["cost"], sampleCount: 2
        }, {
            historyId: "tool:Codex CLI", dbName: "Codex CLI",
            displayName: "Codex CLI", sourceKind: "tool",
            metricKinds: ["usageCount"], sampleCount: 3
        }];

        compare(state.sourceRows.length, 2);
        compare(state.source("provider:OpenAI").statusKey, "disabled");
        verify(state.source("provider:OpenAI").hasHistory);
        compare(state.source("tool:Codex CLI").statusKey, "disabled");
        verify(state.source("tool:Codex CLI").hasHistory);
    }

    function test_unknownRetainedSourceIsHistoryOnly() {
        state.storedCatalog = [{
            historyId: "provider:Legacy Gateway", dbName: "Legacy Gateway",
            displayName: "Legacy Gateway", sourceKind: "provider",
            metricKinds: ["requests"], sampleCount: 1
        }];

        var source = state.source("provider:Legacy Gateway");
        verify(source.historyOnly);
        compare(source.statusKey, "history_only");
        compare(source.displayName, "Legacy Gateway");
    }

    function test_metricsComeOnlyFromStoredCompatibility() {
        state.configuredProviders = [{
            configKey: "openai", dbName: "OpenAI", name: "OpenAI",
            enabled: true
        }, {
            configKey: "anthropic", dbName: "Anthropic", name: "Anthropic",
            enabled: true
        }];
        state.storedCatalog = [{
            historyId: "provider:OpenAI", dbName: "OpenAI",
            displayName: "OpenAI", sourceKind: "provider",
            metricKinds: ["cost", "tokens"], sampleCount: 4
        }];

        compare(state.metricsForSource("provider:OpenAI").length, 2);
        compare(state.metricsForSource("provider:Anthropic").length, 0);
        compare(state.sourcesForMetric("cost").length, 1);
        compare(state.sourcesForMetric("requests").length, 0);
    }

    function test_providerAndToolShareOneNavigationModel() {
        state.configuredProviders = [{
            configKey: "openai", dbName: "OpenAI", name: "OpenAI",
            enabled: true
        }];
        state.configuredTools = [{
            stableId: "codex-cli", name: "Codex CLI", enabled: true
        }];
        state.storedCatalog = [{
            historyId: "provider:OpenAI", dbName: "OpenAI",
            sourceKind: "provider", metricKinds: ["cost"], sampleCount: 1
        }, {
            historyId: "tool:Codex CLI", dbName: "Codex CLI",
            sourceKind: "tool", metricKinds: ["percentUsed"], sampleCount: 1
        }];

        compare(state.sourceRows.length, 2);
        verify(state.availableMetricKinds.indexOf("cost") >= 0);
        verify(state.availableMetricKinds.indexOf("percentUsed") >= 0);
    }

    function test_chartKeepsAvailableZeroAndExplicitGap() {
        var chart = createTemporaryObject(chartComponent, testCase, {
            seriesData: [{
                name: "Zero",
                points: [
                    { timestamp: "2026-01-01T00:00:00Z", value: 0,
                      available: true },
                    { timestamp: "2026-01-01T01:00:00Z", value: null,
                      available: false },
                    { timestamp: "2026-01-01T02:00:00Z", value: 2,
                      available: true }
                ]
            }]
        });
        verify(chart);
        verify(chart.hasData());
        var points = chart.parseSeries()[0].points;
        compare(points.length, 3);
        compare(points[0].v, 0);
        verify(!points[0].gap);
        verify(points[1].gap);
        verify(!points[2].gap);
    }

    function test_chartAcceptsOneDataPoint() {
        var chart = createTemporaryObject(chartComponent, testCase, {
            seriesData: [{
                name: "Single",
                points: [{
                    timestamp: "2026-01-01T00:00:00Z",
                    value: 4,
                    available: true
                }]
            }]
        });
        verify(chart);
        verify(chart.hasData());
        compare(chart.parseSeries()[0].points.length, 1);
        verify(!chart.showLegend);
    }

    function test_chartMeasuresAxisMarginsAndShowsComparisonLegend() {
        var chart = createTemporaryObject(chartComponent, testCase, {
            seriesData: [{
                name: "Actual",
                currency: "USD",
                points: [
                    { timestamp: "2026-01-01T00:00:00Z",
                      value: 12345.67, available: true },
                    { timestamp: "2026-01-08T00:00:00Z",
                      value: 15000, available: true }
                ]
            }, {
                name: "Estimated",
                currency: "USD",
                points: [
                    { timestamp: "2026-01-01T00:00:00Z",
                      value: 8000, available: true },
                    { timestamp: "2026-01-08T00:00:00Z",
                      value: null, available: false }
                ]
            }]
        });
        verify(chart);
        verify(chart.showLegend);
        verify(chart.marginLeft > 50);
        verify(chart.marginRight > 12);
        verify(chart.firstXAxisLabel().length > 0);
        verify(chart.lastXAxisLabel().length > 0);
    }

    function test_moneyUsesCurrencyCodeAndLocaleDecimalSeparator() {
        compare(Utils.formatMoney(2.07, "usd", Qt.locale("en_US")),
                "USD\u00a02.07");
        compare(Utils.formatMoney(2.07, "usd", Qt.locale("sv_SE")),
                "USD\u00a02,07");
    }
}
