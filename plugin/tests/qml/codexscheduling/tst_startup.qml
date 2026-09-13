import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor

TestCase {
    id: testCase
    name: "CodexStartupScheduling"

    property int subscriptionSyncCalls: 0
    property int installChecks: 0

    QtObject {
        id: configuration
        property bool browserSyncEnabled: false
        property bool openaiEnabled: false
        property bool anthropicEnabled: false
        property bool googleEnabled: false
        property bool mistralEnabled: false
        property bool deepseekEnabled: false
        property bool groqEnabled: false
        property bool xaiEnabled: false
        property bool ollamaEnabled: false
        property bool openrouterEnabled: false
        property bool togetherEnabled: false
        property bool cohereEnabled: false
        property bool googleveoEnabled: false
        property bool azureEnabled: false
        property bool bedrockEnabled: false
        property bool litellmEnabled: false
        property bool cerebrasEnabled: false
        property bool fireworksEnabled: false
        property bool perplexityEnabled: false
        property string litellmModel: ""
        property string cerebrasModel: ""
        property string fireworksModel: ""
        property string perplexityModel: ""
        property string litellmCustomBaseUrl: ""
        property string cerebrasCustomBaseUrl: ""
        property string fireworksCustomBaseUrl: ""
        property string perplexityCustomBaseUrl: ""
        property bool codexEnabled: false
        property bool claudeCodeEnabled: false
        property bool copilotEnabled: false
        property string copilotOrgName: ""
        property bool cursorEnabled: false
        property bool windsurfEnabled: false
        property bool jetbrainsAiEnabled: false
        property bool antigravityEnabled: false
        property bool forecastUiEnabled: false
        property bool historyEnabled: false
    }

    QtObject {
        id: registry
        property var allProviders: []
        property var allSubscriptionTools: []
        property bool demoMode: false
        function providerByConfigKey(configKey) { return null; }
    }

    QtObject {
        id: secrets
        property bool walletOpen: false
        signal keyStored(string provider)
        signal keyRemoved(string provider)
        function getKey(provider) { return ""; }
    }

    QtObject {
        id: database
        property bool enabled: false
        signal observationsChanged()
        function init() {}
        function pruneOldData() {}
    }

    QtObject {
        id: guardrails
        property var forecasts: []
        function invalidateCache() {}
        function refreshWithQuery(query) {}
    }

    QtObject {
        id: budgetPolicy
        property int revision: 0
        function queryPolicies() { return []; }
    }

    QtObject {
        id: scheduler
        property int refreshStartup: 0
        property int refreshConfigurationChanged: 4
        property int refreshCredentialChanged: 5
        function refreshAll(reason) {}
        function refreshProvider(provider, reason, force) {}
        function performAutomaticSubscriptionSync() {
            testCase.subscriptionSyncCalls++;
        }
    }

    QtObject {
        id: monitor
        property bool enabled: configuration.codexEnabled
        property var lastSuccessfulRefresh: null
        function checkToolInstalled() { testCase.installChecks++; }
        function refreshQuota() {}
    }

    QtObject {
        id: metricsServer
        property string payload: ""
    }

    QtObject {
        id: webhookNotifier
        property string slackWebhookUrl: ""
        property string discordWebhookUrl: ""
    }

    Component {
        id: runtimeComponent
        Monitor.RuntimeCoordinator {}
    }

    function init() {
        subscriptionSyncCalls = 0;
        installChecks = 0;
        configuration.browserSyncEnabled = false;
        configuration.codexEnabled = false;
    }

    function createRuntime() {
        return createTemporaryObject(runtimeComponent, testCase, {
            configuration: configuration,
            registry: registry,
            secrets: secrets,
            usageDatabase: database,
            guardrailModel: guardrails,
            budgetPolicyRuntime: budgetPolicy,
            scheduler: scheduler,
            metricsServer: metricsServer,
            webhookNotifier: webhookNotifier,
            claudeCodeMonitor: monitor,
            codexCliMonitor: monitor,
            copilotMonitor: monitor,
            cursorMonitor: monitor,
            windsurfMonitor: monitor,
            jetbrainsAiMonitor: monitor,
            antigravityMonitor: monitor
        });
    }

    function oneShotTimerWithInterval(owner, interval) {
        for (var index = 0; index < owner.data.length; ++index) {
            var candidate = owner.data[index];
            if (candidate && !candidate.repeat && candidate.interval === interval)
                return candidate;
        }
        return null;
    }

    function test_startupSchedulesCodexWithoutBrowserSync() {
        configuration.codexEnabled = true;
        var runtime = createRuntime();
        verify(runtime);
        var timer = oneShotTimerWithInterval(runtime, 5000);
        verify(timer);
        compare(timer.running, true);

        timer.interval = 1;
        tryCompare(testCase, "subscriptionSyncCalls", 1);
    }

    function test_enablingCodexTriggersPromptSync() {
        var runtime = createRuntime();
        verify(runtime);

        configuration.codexEnabled = true;

        tryCompare(testCase, "subscriptionSyncCalls", 1);
        verify(installChecks >= 1);
    }
}
