import QtQuick
import QtTest
import "../../../../package/contents/ui/onboarding" as Onboarding

TestCase {
    id: testCase
    name: "GuidedSetupController"

    QtObject {
        id: fakeConfiguration
        property bool setupWizardCompleted: false
        property bool setupWizardDismissed: false
        property bool setupWizardInProgress: false
        property int setupWizardStep: 0
        property string setupWizardGoal: ""
        property string setupWizardSourceId: ""
        property bool openaiEnabled: false
        property bool codexEnabled: false
        property string openaiCustomBaseUrl: ""
    }

    QtObject {
        id: fakeStore
        property bool walletOpen: true
        property bool rejectWrites: false
        property var values: ({})
        property var storeCalls: []

        function hasKey(key) { return !!values[key]; }
        function storeKey(key, value) {
            storeCalls = storeCalls.concat([{ key: key, value: value }]);
            if (rejectWrites) return false;
            var next = Object.assign({}, values);
            next[key] = value;
            values = next;
            return true;
        }
        function removeKey(key) {
            var next = Object.assign({}, values);
            delete next[key];
            values = next;
            return true;
        }
    }

    QtObject {
        id: fakeReadiness
        property var sources: ({})
        property var outcomes: ({})
        property int verifyCalls: 0
        signal sourceChanged(string stableId)

        function source(stableId) { return sources[stableId] || ({}); }
        function rankedSourceIds() { return ["codex-cli", "openai"]; }
        function update(stableId, values) {
            var nextSources = Object.assign({}, sources);
            nextSources[stableId] = Object.assign({}, nextSources[stableId], values);
            sources = nextSources;
            sourceChanged(stableId);
        }
        function verifySource(stableId) {
            verifyCalls++;
            var state = outcomes[stableId] || (stableId === "codex-cli" ? "reporting_estimate" : "reporting_actual");
            update(stableId, {
                readinessStateKey: state,
                nextActionText: state === "failed" ? "Replace the credential and try again." : ""
            });
            return state !== "failed";
        }
    }

    QtObject {
        id: fakeSourceApi
        property int enableCalls: 0
        property int endpointCalls: 0

        function setGuidedSourceEnabled(stableId, enabled) {
            enableCalls++;
            if (stableId === "openai") fakeConfiguration.openaiEnabled = enabled;
            if (stableId === "codex-cli") fakeConfiguration.codexEnabled = enabled;
            fakeReadiness.update(stableId, { enabled: enabled, readinessStateKey: "ready_to_verify" });
            return true;
        }
        function setGuidedSourceEndpoint(stableId, endpoint) {
            endpointCalls++;
            if (stableId === "openai") fakeConfiguration.openaiCustomBaseUrl = endpoint;
            return true;
        }
        function hasGuidedSourceEndpoint(stableId) {
            return stableId === "openai" && fakeConfiguration.openaiCustomBaseUrl.length > 0;
        }
        function verifyGuidedSource(stableId) { return fakeReadiness.verifySource(stableId); }
    }

    Onboarding.GuidedSetupController {
        id: controller
        readinessModel: fakeReadiness
        secretStore: fakeStore
        configuration: fakeConfiguration
        sourceApi: fakeSourceApi
    }

    function baseSources() {
        return {
            "codex-cli": {
                stableId: "codex-cli", displayName: "Codex CLI", sourceKindKey: "local_tool",
                monitoringLevel: "local_activity_estimate", requiredCredentialSlots: [], installed: true,
                safeVerification: true, customEndpointRequired: false, readinessStateKey: "disabled"
            },
            "openai": {
                stableId: "openai", displayName: "OpenAI", sourceKindKey: "provider",
                monitoringLevel: "actual_usage_spend", requiredCredentialSlots: ["openai"], installed: true,
                safeVerification: true, customEndpointRequired: false, readinessStateKey: "disabled"
            }
        };
    }

    function init() {
        controller.verificationTimeout.stop();
        fakeConfiguration.setupWizardCompleted = false;
        fakeConfiguration.setupWizardDismissed = false;
        fakeConfiguration.setupWizardInProgress = false;
        fakeConfiguration.setupWizardStep = 0;
        fakeConfiguration.setupWizardGoal = "";
        fakeConfiguration.setupWizardSourceId = "";
        fakeConfiguration.openaiEnabled = false;
        fakeConfiguration.codexEnabled = false;
        fakeConfiguration.openaiCustomBaseUrl = "";
        fakeStore.walletOpen = true;
        fakeStore.rejectWrites = false;
        fakeStore.values = ({});
        fakeStore.storeCalls = [];
        fakeReadiness.sources = baseSources();
        fakeReadiness.outcomes = ({});
        fakeReadiness.verifyCalls = 0;
        fakeSourceApi.enableCalls = 0;
        fakeSourceApi.endpointCalls = 0;
        controller.initialize();
        controller.statusMessage = "";
        controller.statusError = false;
    }

    function test_localToolSuccess() {
        controller.chooseGoal("local");
        compare(controller.candidates[0].stableId, "codex-cli");
        controller.selectSource("codex-cli", true);
        verify(controller.saveAndVerify());

        tryCompare(controller, "step", controller.resultStep);
        verify(fakeConfiguration.codexEnabled);
        verify(fakeConfiguration.setupWizardCompleted);
        compare(controller.resultQuality, "Local activity estimate");
        compare(fakeReadiness.verifyCalls, 1);
    }

    function test_providerSuccessStoresCredential() {
        controller.chooseGoal("usage");
        controller.selectSource("openai", true);
        controller.setCredential("openai", "test-admin-key");
        verify(controller.saveAndVerify());

        tryCompare(controller, "step", controller.resultStep);
        compare(fakeStore.storeCalls.length, 1);
        compare(fakeStore.storeCalls[0].key, "openai");
        verify(fakeConfiguration.openaiEnabled);
        compare(controller.resultQuality, "Actual usage and spend");
    }

    function test_authFailureStaysInContext() {
        fakeReadiness.outcomes = ({ "openai": "failed" });
        controller.chooseGoal("usage");
        controller.selectSource("openai", true);
        controller.setCredential("openai", "bad-key");
        verify(controller.saveAndVerify());

        tryVerify(function() { return controller.statusError; });
        compare(controller.step, controller.verificationStep);
        compare(controller.statusMessage, "Replace the credential and try again.");
        verify(!fakeConfiguration.setupWizardCompleted);
    }

    function test_walletFailureDoesNotEnableSource() {
        fakeStore.rejectWrites = true;
        controller.chooseGoal("usage");
        controller.selectSource("openai", true);
        controller.setCredential("openai", "test-key");

        verify(!controller.saveAndVerify());
        verify(controller.statusError);
        compare(controller.step, controller.configureStep);
        compare(fakeSourceApi.enableCalls, 0);
        verify(!fakeConfiguration.openaiEnabled);

        fakeStore.rejectWrites = false;
        verify(controller.saveAndVerify());
        tryCompare(controller, "step", controller.resultStep);
        verify(fakeConfiguration.openaiEnabled);
    }

    function test_requiredEndpointIsValidatedBeforeCredentialMutation() {
        fakeReadiness.update("openai", { customEndpointRequired: true });
        controller.chooseGoal("usage");
        controller.selectSource("openai", true);
        controller.setCredential("openai", "test-key");

        verify(!controller.saveAndVerify());
        compare(fakeStore.storeCalls.length, 0);
        controller.customEndpoint = "not-a-url";
        verify(!controller.saveAndVerify());
        compare(fakeStore.storeCalls.length, 0);

        controller.customEndpoint = "https://gateway.example.com";
        verify(controller.saveAndVerify());
        tryCompare(controller, "step", controller.resultStep);
        compare(fakeSourceApi.endpointCalls, 1);
        compare(fakeConfiguration.openaiCustomBaseUrl, "https://gateway.example.com");
    }

    function test_skipAndResumeAreDistinctFromCompletion() {
        controller.chooseGoal("local");
        controller.selectSource("codex-cli", true);
        controller.skip();

        compare(controller.step, controller.pausedStep);
        verify(fakeConfiguration.setupWizardDismissed);
        verify(!fakeConfiguration.setupWizardCompleted);

        controller.resume();
        compare(controller.step, controller.configureStep);
        verify(fakeConfiguration.setupWizardInProgress);
        verify(!fakeConfiguration.setupWizardDismissed);
    }

    function test_completionSurvivesRestartAndCanRunAgain() {
        controller.chooseGoal("local");
        controller.selectSource("codex-cli", true);
        verify(controller.saveAndVerify());
        tryCompare(controller, "step", controller.resultStep);
        controller.finish();

        verify(fakeConfiguration.setupWizardCompleted);
        verify(!fakeConfiguration.setupWizardInProgress);
        controller.initialize();
        compare(controller.step, controller.resultStep);

        controller.startAgain();
        compare(controller.step, controller.goalStep);
        verify(fakeConfiguration.setupWizardCompleted);
        verify(fakeConfiguration.setupWizardInProgress);
        verify(fakeConfiguration.codexEnabled);
    }
}
