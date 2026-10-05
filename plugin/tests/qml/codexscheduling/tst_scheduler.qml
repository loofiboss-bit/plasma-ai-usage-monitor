import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor

TestCase {
    id: testCase
    name: "CodexSchedulerPrivacy"

    property int browserCalls: 0
    property var browserServices: []
    property int codexLocalCalls: 0
    property int genericGateCalls: 0
    property int localAuthGateCalls: 0
    property int antigravityCalls: 0

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
        property bool syncNeedsAction: false
        property bool syncing: false
        property date lastSyncTime: new Date(NaN)
        property date lastSyncCompletionTime: new Date(NaN)
        property date syncRetryAfter: new Date(NaN)
        property date nextScheduledRefresh: new Date(NaN)
        signal syncStatusChanged()
        function canAutoSync() { testCase.genericGateCalls++; return true; }
        function canAutoSyncFromLocalAuth() { testCase.localAuthGateCalls++; return true; }
        function syncFromLocalAuth() {
            testCase.codexLocalCalls++;
            lastSyncTime = new Date();
            lastSyncCompletionTime = lastSyncTime;
            syncStatusChanged();
        }
        function setNextScheduledRefresh(value) { nextScheduledRefresh = value; }
    }

    QtObject {
        id: claudeMonitor
        property bool installed: true
        property bool syncNeedsAction: false
        property bool syncing: false
        property date lastSyncTime: new Date(NaN)
        property date lastSyncCompletionTime: new Date(NaN)
        property date syncRetryAfter: new Date(NaN)
        property date nextScheduledRefresh: new Date(NaN)
        signal syncStatusChanged()
        function canAutoSync() { return true; }
        function setNextScheduledRefresh(value) { nextScheduledRefresh = value; }
    }

    QtObject {
        id: antigravityMonitor
        property bool installed: true
        property bool syncing: false
        property bool syncNeedsAction: false
        property string readinessCode: "ready"
        property date lastSyncTime: new Date()
        property date lastSyncCompletionTime: new Date(NaN)
        property date syncRetryAfter: new Date(NaN)
        property date lastSuccessfulRefresh: new Date(0)
        property date nextScheduledRefresh: new Date(NaN)
        signal syncStatusChanged()
        signal antigravityStatusChanged()
        signal syncCompleted(bool success)
        function setNextScheduledRefresh(value) { nextScheduledRefresh = value; }
        function refreshQuota() { testCase.antigravityCalls++; }
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
        id: scheduledBackend
        property date lastSuccess: new Date(Date.now() - 60 * 1000)
        property date nextScheduledRefresh: new Date(0)
        signal stateChanged()
        function setNextScheduledRefresh(value) { nextScheduledRefresh = value; }
        function requestRefresh() {}
    }

    QtObject {
        id: scheduledProvider
        property string configKey: "openai"
        property bool enabled: true
        property int refreshInterval: 300
        property int minimumRefreshSeconds: 300
        property bool requiresApiKey: false
        property var backend: scheduledBackend
    }

    QtObject {
        id: database
        function requestPrune(requestId) {}
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
        genericGateCalls = 0;
        localAuthGateCalls = 0;
        antigravityCalls = 0;
        codexMonitor.syncNeedsAction = false;
        codexMonitor.syncing = false;
        codexMonitor.lastSyncTime = new Date(NaN);
        codexMonitor.lastSyncCompletionTime = new Date(NaN);
        codexMonitor.syncRetryAfter = new Date(NaN);
        codexMonitor.nextScheduledRefresh = new Date(NaN);
        claudeMonitor.syncNeedsAction = false;
        claudeMonitor.syncing = false;
        claudeMonitor.lastSyncTime = new Date(NaN);
        claudeMonitor.lastSyncCompletionTime = new Date(NaN);
        claudeMonitor.syncRetryAfter = new Date(NaN);
        claudeMonitor.nextScheduledRefresh = new Date(NaN);
        configuration.antigravityEnabled = false;
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
            antigravityMonitor: antigravityMonitor,
            usageDatabase: database,
            popupOpen: false
        });
    }

    function scheduledTimer(owner) {
        for (var index = 0; index < owner.data.length; ++index) {
            var candidate = owner.data[index];
            if (candidate && candidate.repeat === false && candidate.dueAt
                    && isFinite(candidate.dueAt.getTime()))
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
        compare(localAuthGateCalls, 1);
        compare(genericGateCalls, 0);
    }

    function test_explicitBrowserSyncAllowsCodexFallbackAndGatesClaude() {
        configuration.codexEnabled = true;
        configuration.claudeCodeEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);

        scheduler.performBrowserSync();
        compare(browserCalls, 0);
        compare(genericGateCalls, 0);
        compare(localAuthGateCalls, 0);

        configuration.browserSyncEnabled = true;
        scheduler.performBrowserSync();
        compare(browserCalls, 2);
        verify(browserServices.indexOf("claude") >= 0);
        verify(browserServices.indexOf("codex") >= 0);
        compare(codexLocalCalls, 0);
        compare(genericGateCalls, 1);
        compare(localAuthGateCalls, 0);
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
        compare(localAuthGateCalls, 1);
        compare(genericGateCalls, 0);
    }

    function test_singleShotTimerRunsLocalAuthWithoutBrowserSync() {
        configuration.codexEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);
        var timer = scheduledTimer(scheduler);
        verify(timer);
        compare(timer.running, true);
        compare(timer.repeat, false);
        var dueBefore = timer.dueAt;

        wait(10);
        timer.triggered();
        compare(codexLocalCalls, 1);
        compare(browserCalls, 0);
        compare(localAuthGateCalls, 1);
        compare(genericGateCalls, 0);
        verify(timer.dueAt > dueBefore);
        compare(codexMonitor.nextScheduledRefresh.getTime(), timer.dueAt.getTime());
    }

    function test_retryAfterAndActionNeededControlNextDeadline() {
        configuration.codexEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);
        var timer = scheduledTimer(scheduler);
        verify(timer);

        var retryAt = new Date(Date.now() + 5 * 60 * 1000);
        codexMonitor.lastSyncCompletionTime = new Date();
        codexMonitor.syncRetryAfter = retryAt;
        codexMonitor.syncStatusChanged();
        verify(timer.dueAt >= retryAt);

        codexMonitor.syncNeedsAction = true;
        codexMonitor.syncStatusChanged();
        compare(timer.running, false);
        verify(!isFinite(timer.dueAt.getTime()));
        verify(!isFinite(codexMonitor.nextScheduledRefresh.getTime()));
    }

    function test_offlinePausesExternalAutomaticSyncButKeepsLocalAndManualSync() {
        configuration.browserSyncEnabled = true;
        configuration.claudeCodeEnabled = true;
        configuration.codexEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);

        scheduler.networkPolicy.observeReachability(false);
        scheduler.performAutomaticSubscriptionSync();
        compare(browserCalls, 0);
        compare(codexLocalCalls, 1);

        scheduler.performBrowserSync();
        compare(browserCalls, 2);
        verify(browserServices.indexOf("claude") >= 0);
        verify(browserServices.indexOf("codex") >= 0);
    }

    function test_offlineAntigravityTimerDoesNotCallNetworkAndRecoversOnce() {
        configuration.antigravityEnabled = true;
        var scheduler = createScheduler();
        verify(scheduler);
        var timer = scheduledTimer(scheduler);
        verify(timer);

        scheduler.networkPolicy.observeReachability(false);
        compare(timer.running, false);
        verify(!isFinite(antigravityMonitor.nextScheduledRefresh.getTime()));
        timer.triggered();
        compare(antigravityCalls, 0);

        scheduler.networkPolicy.observeReachability(true);
        tryCompare(testCase, "antigravityCalls", 1, 2000);
        compare(antigravityCalls, 1);
    }

    function test_nextScheduledRefreshAdvancesAfterSuccessfulRefresh() {
        configuration.refreshInterval = 300;
        registry.allProviders = [scheduledProvider];
        var scheduler = createScheduler();
        verify(scheduler);
        var previousNext = scheduledBackend.nextScheduledRefresh;

        scheduledBackend.lastSuccess = new Date(Date.now());
        scheduledBackend.stateChanged();
        verify(scheduledBackend.nextScheduledRefresh > scheduledBackend.lastSuccess);
        verify(scheduledBackend.nextScheduledRefresh > previousNext);
    }
}
