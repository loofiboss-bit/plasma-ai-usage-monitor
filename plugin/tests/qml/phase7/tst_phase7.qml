import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "Phase7Accessibility"

    function i18n(message) {
        var result = message;
        for (var i = 1; i < arguments.length; ++i)
            result = result.replace("%" + i, arguments[i]);
        return result;
    }

    function i18np(singular, plural, count) {
        var args = [count === 1 ? singular : plural];
        for (var i = 2; i < arguments.length; ++i) args.push(arguments[i]);
        if (args.length === 1) args.push(count);
        return i18n.apply(null, args);
    }

    Components.CompactMetricState {
        id: compactState
    }

    property int networkRefreshCount: 0
    property int lastRefreshReason: -1

    QtObject {
        id: schedulerConfiguration
        property int refreshInterval: 60
        property bool browserSyncEnabled: false
        property bool claudeCodeEnabled: false
        property bool codexEnabled: false
        property int browserSyncInterval: 300
        property bool antigravityEnabled: false
        property int antigravityRefreshInterval: 300
        property bool autoExportEnabled: false
        property string autoExportDirectory: ""
        property string autoExportFormat: "json"
        property int autoExportIntervalMinutes: 60
        property bool copilotEnabled: false
    }

    QtObject {
        id: schedulerBackend
        property var lastSuccess: new Date()
        property int consecutiveErrors: 0
        property bool retryable: false
        function hasApiKey() { return true; }
        function requestRefresh(reason) {
            testCase.networkRefreshCount++;
            testCase.lastRefreshReason = reason;
            lastSuccess = new Date();
        }
        function setNextScheduledRefresh(value) {}
    }

    QtObject {
        id: schedulerRegistry
        property var allProviders: [{
            configKey: "phase7",
            enabled: true,
            requiresApiKey: true,
            refreshInterval: 60,
            minimumRefreshSeconds: 0,
            backend: schedulerBackend
        }]
    }

    QtObject {
        id: noOpMonitor
        property bool installed: false
        property var lastSuccessfulRefresh: null
        function refreshQuota() {}
    }

    QtObject {
        id: noOpService
        function sync(name, monitor) {}
    }

    QtObject {
        id: noOpDatabase
        function pruneOldData() {}
        function requestExportAll(requestId, directory, formats) {}
    }

    Component {
        id: schedulerComponent
        Monitor.RefreshScheduler {}
    }

    Component {
        id: sourceCardComponent
        Monitor.DailySourceCard {
            width: 320
            row: ({})
        }
    }

    Component {
        id: chartComponent
        Monitor.MultiSeriesChart {
            width: 320
            height: 180
        }
    }

    function init() {
        compactState.summary = {};
        networkRefreshCount = 0;
        lastRefreshReason = -1;
        schedulerBackend.lastSuccess = new Date();
    }

    function test_compactScreenReaderSummaryNamesSeverityAndSource() {
        compactState.summary = {
            enabledSourceCount: 2,
            reportingUsefulSourceCount: 1,
            attentionSourceCount: 1,
            highestSeverity: "critical",
            mostUrgentSource: {
                stableId: "openai",
                displayName: "OpenAI"
            }
        };

        var text = compactState.summaryText();
        verify(text.indexOf("Critical: OpenAI") >= 0);
        verify(text.indexOf("1 of 2 active sources") >= 0);
    }

    function test_warningAndCriticalAreNotColorOnly() {
        var critical = createTemporaryObject(sourceCardComponent, testCase, {
            row: {
                stableId: "openai",
                displayName: "OpenAI",
                sourceKind: "provider",
                qualityClass: "actual",
                freshnessState: "fresh",
                attentionSeverity: "critical",
                nextActionKey: "review_quota",
                primaryMetricAvailable: true,
                primaryMetricValue: 4,
                primaryMetricUnit: "percent_remaining"
            }
        });
        verify(critical);
        compare(critical.attentionText(), "Critical · ");
        verify(critical.Accessible.name.indexOf("Critical") >= 0);
        verify(critical.Accessible.name.indexOf("OpenAI") >= 0);

        critical.row = Object.assign({}, critical.row, {
            attentionSeverity: "warning"
        });
        compare(critical.attentionText(), "Warning · ");
        verify(critical.Accessible.name.indexOf("Warning") >= 0);
    }

    function test_chartSummaryExposesSeriesCoverageAndGaps() {
        var chart = createTemporaryObject(chartComponent, testCase, {
            metric: "cost",
            seriesData: [{
                name: "OpenAI",
                color: "#10a37f",
                points: [
                    { timestamp: "2026-07-01T00:00:00Z",
                      available: true, value: 0 },
                    { timestamp: "2026-07-02T00:00:00Z",
                      available: false, value: null },
                    { timestamp: "2026-07-03T00:00:00Z",
                      available: true, value: 2 }
                ]
            }]
        });
        verify(chart);
        var summary = chart.accessibleSummary();
        verify(summary.indexOf("OpenAI") >= 0);
        verify(summary.indexOf("2 recorded points") >= 0);
        verify(summary.indexOf("1 gaps") >= 0);
    }

    function test_longLocalizedSourceTextFitsNarrowCardContract() {
        var card = createTemporaryObject(sourceCardComponent, testCase, {
            width: 280,
            row: {
                stableId: "long-source",
                displayName: "Extremely long localized source name used for narrow popup validation",
                sourceKind: "provider",
                qualityClass: "unavailable",
                freshnessState: "stale",
                attentionSeverity: "warning",
                nextActionKey: "refresh_stale_data",
                primaryMetricAvailable: false
            }
        });
        verify(card);
        verify(card.implicitHeight > 0);
        verify(card.width === 280);
        verify(card.Accessible.name.indexOf("Extremely long localized") >= 0);
        verify(card.Accessible.name.indexOf("Warning") >= 0);
    }

    function test_freshPopupDoesNotIssueNetworkRefresh() {
        var scheduler = createTemporaryObject(schedulerComponent, testCase, {
            configuration: schedulerConfiguration,
            registry: schedulerRegistry,
            browserSyncService: noOpService,
            claudeCodeMonitor: noOpMonitor,
            codexCliMonitor: noOpMonitor,
            copilotMonitor: noOpMonitor,
            antigravityMonitor: noOpMonitor,
            usageDatabase: noOpDatabase,
            popupOpen: false
        });
        verify(scheduler);

        scheduler.popupOpen = true;
        wait(0);
        compare(networkRefreshCount, 0);
    }

    function test_stalePopupIssuesOneNetworkRefresh() {
        schedulerBackend.lastSuccess = new Date(0);
        var scheduler = createTemporaryObject(schedulerComponent, testCase, {
            configuration: schedulerConfiguration,
            registry: schedulerRegistry,
            browserSyncService: noOpService,
            claudeCodeMonitor: noOpMonitor,
            codexCliMonitor: noOpMonitor,
            copilotMonitor: noOpMonitor,
            antigravityMonitor: noOpMonitor,
            usageDatabase: noOpDatabase,
            popupOpen: false
        });
        verify(scheduler);

        scheduler.popupOpen = true;
        tryCompare(testCase, "networkRefreshCount", 1);
        compare(lastRefreshReason, scheduler.refreshPopupOpened);
    }
}
