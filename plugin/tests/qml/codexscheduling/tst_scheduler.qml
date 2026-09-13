import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor

TestCase {
    id: testCase
    name: "CodexSchedulerPrivacy"

    property int browserCalls: 0
    property var browserServices: []
    property int codexLocalCalls: 0

    QtObject {
        id: configuration
        property int refreshInterval: 60
        property bool browserSyncEnabled: false
        property bool claudeCodeEnabled: false
        property bool codexEnabled: false
        property int browserSyncInterval: 61
        property bool antigravityEnabled: false
        property int antigravityRefreshInterval: 300
        property bool autoExportEnabled: false
        property string autoExportDirectory: ""
        property string autoExportFormat: "json"
        property int autoExportIntervalMinutes: 60
        property bool copilotEnabled: false
    }

    QtObject {
        id: codexMonitor
        property bool installed: true
        function canAutoSync() { return true; }
        function syncFromLocalAuth() { testCase.codexLocalCalls++; }
    }

    QtObject {
        id: claudeMonitor
        property bool installed: true
        function canAutoSync() { return true; }
    }

    QtObject {
        id: browserService
        function sync(service, monitor) {
            testCase.browserCalls++;
            testCase.browserServices.push(service);
            return false;
        }
    }

    QtObject {
        id: noOpMonitor
        property bool installed: false
        property var lastSuccessfulRefresh: null
        function refreshQuota() {}
    }

    QtObject {
        id: registry
        property var allProviders: []
    }

    QtObject {
        id: database
        function pruneOldData() {}
        function requestExportAll(requestId, directory, formats) {}
    }

    Component {
        id: schedulerComponent
        Monitor.RefreshScheduler {}
    }

    function init() {
        browserCalls = 0;
        browserServices = [];
        codexLocalCalls = 0;
        configuration.browserSyncEnabled = false;
        configuration.claudeCodeEnabled = false;
        configuration.codexEnabled = false;
        configuration.browserSyncInterval = 61;
    }

    function createScheduler() {
        return createTemporaryObject(schedulerComponent, testCase, {
            configuration: configuration,
            registry: registry,
            browserSyncService: browserService,
            claudeCodeMonitor: claudeMonitor,
            codexCliMonitor: codexMonitor,
            copilotMonitor: noOpMonitor,
            antigravityMonitor: noOpMonitor,
            usageDatabase: database,
            popupOpen: false
        });
    }

    function repeatingTimerWithInterval(owner, interval) {
        for (var index = 0; index < owner.data.length; ++index) {
            var candidate = owner.data[index];
            if (candidate && candidate.repeat && candidate.interval === interval)
                return candidate;
        }
        return null;
    }

    function test_automaticSyncUsesLocalAuthWithoutBrowserAccess() {
        configuration.codexEnabled = true;
        configuration.claudeCodeEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);

        scheduler.performAutomaticSubscriptionSync();

        compare(codexLocalCalls, 1);
        compare(browserCalls, 0);
    }

    function test_explicitBrowserSyncAllowsCodexFallbackAndGatesClaude() {
        configuration.codexEnabled = true;
        configuration.claudeCodeEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);

        scheduler.performBrowserSync();
        compare(browserCalls, 0);

        configuration.browserSyncEnabled = true;
        scheduler.performBrowserSync();
        compare(browserCalls, 2);
        verify(browserServices.indexOf("claude") >= 0);
        verify(browserServices.indexOf("codex") >= 0);
        compare(codexLocalCalls, 0);
    }

    function test_automaticCodexIgnoresBrowserServiceCircuit() {
        configuration.browserSyncEnabled = true;
        configuration.codexEnabled = true;
        configuration.claudeCodeEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);

        scheduler.performAutomaticSubscriptionSync();

        compare(browserCalls, 1);
        compare(browserServices[0], "claude");
        compare(codexLocalCalls, 1);
    }

    function test_periodicTimerRunsLocalAuthWithoutBrowserSync() {
        configuration.codexEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);
        var timer = repeatingTimerWithInterval(scheduler, 61000);
        verify(timer);
        compare(timer.running, true);

        timer.triggered();
        compare(codexLocalCalls, 1);
        compare(browserCalls, 0);
    }
}
