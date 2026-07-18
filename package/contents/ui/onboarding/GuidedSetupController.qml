import QtQuick
import ".." as AppUi

QtObject {
    id: controller

    readonly property int goalStep: 0
    readonly property int sourceStep: 1
    readonly property int configureStep: 2
    readonly property int verificationStep: 3
    readonly property int resultStep: 4
    readonly property int pausedStep: 5

    required property var readinessModel
    required property var secretStore
    required property var configuration
    required property var sourceApi

    property int step: goalStep
    property string goal: ""
    property string selectedSourceId: ""
    property var selectedSource: ({})
    property var recommendedSource: ({})
    property var candidates: []
    property var credentialValues: ({})
    property string customEndpoint: ""
    property string statusMessage: ""
    property bool statusError: false
    property string resultQuality: ""
    property string resultSummary: ""

    readonly property var requiredCredentialSlots: selectedSource.requiredCredentialSlots || []
    readonly property bool needsCustomEndpoint: !!selectedSource.customEndpointRequired
    readonly property bool busy: step === verificationStep
    readonly property bool paused: step === pausedStep

    signal finished()
    signal dismissed()

    property AppUi.SecretChangeSet secretChanges: AppUi.SecretChangeSet {
        store: controller.secretStore
    }

    property Connections readinessConnection: Connections {
        target: controller.readinessModel

        function onSourceChanged(stableId) {
            controller.refreshRecommendation();
            if (controller.goal) controller.refreshCandidates();
            if (stableId !== controller.selectedSourceId) return;
            controller.refreshSelectedSource();
            controller.evaluateVerification();
        }
    }

    property Timer verificationTimeout: Timer {
        interval: 30000
        repeat: false
        onTriggered: {
            if (controller.step !== controller.verificationStep) return;
            controller.statusError = true;
            controller.statusMessage = qsTr("Verification timed out. Check the network and try again.");
        }
    }

    function initialize() {
        goal = configuration.setupWizardGoal || "";
        selectedSourceId = configuration.setupWizardSourceId || "";
        var savedStep = Number(configuration.setupWizardStep || 0);
        step = configuration.setupWizardDismissed && !configuration.setupWizardInProgress
             ? pausedStep : Math.max(goalStep, Math.min(resultStep, savedStep));
        refreshSelectedSource();
        refreshRecommendation();
        refreshCandidates();
        if (step === verificationStep) evaluateVerification();
    }

    function startAgain() {
        configuration.setupWizardDismissed = false;
        configuration.setupWizardInProgress = true;
        configuration.setupWizardStep = goalStep;
        configuration.setupWizardGoal = "";
        configuration.setupWizardSourceId = "";
        goal = "";
        selectedSourceId = "";
        selectedSource = ({});
        refreshRecommendation();
        candidates = [];
        credentialValues = ({});
        customEndpoint = "";
        statusMessage = "";
        statusError = false;
        step = goalStep;
    }

    function resume() {
        configuration.setupWizardDismissed = false;
        configuration.setupWizardInProgress = true;
        var savedStep = Number(configuration.setupWizardStep || goalStep);
        step = Math.max(goalStep, Math.min(resultStep, savedStep));
        refreshSelectedSource();
        refreshCandidates();
    }

    function skip() {
        if (step !== pausedStep) configuration.setupWizardStep = step;
        configuration.setupWizardDismissed = true;
        configuration.setupWizardInProgress = false;
        step = pausedStep;
        verificationTimeout.stop();
        dismissed();
    }

    function chooseGoal(newGoal) {
        goal = newGoal;
        configuration.setupWizardGoal = newGoal;
        configuration.setupWizardInProgress = true;
        configuration.setupWizardDismissed = false;
        goTo(sourceStep);
        refreshCandidates();
        if (candidates.length > 0) selectSource(candidates[0].stableId, false);
    }

    function selectSource(stableId, advance) {
        selectedSourceId = stableId;
        configuration.setupWizardSourceId = stableId;
        refreshSelectedSource();
        credentialValues = ({});
        customEndpoint = "";
        statusMessage = "";
        statusError = false;
        if (advance === undefined || advance) goTo(configureStep);
    }

    function back() {
        if (step === configureStep) goTo(sourceStep);
        else if (step === sourceStep) goTo(goalStep);
        else if (step === verificationStep) goTo(configureStep);
    }

    function goTo(nextStep) {
        step = nextStep;
        configuration.setupWizardStep = nextStep;
    }

    function refreshSelectedSource() {
        selectedSource = selectedSourceId && readinessModel
                       ? readinessModel.source(selectedSourceId) : ({});
    }

    function refreshRecommendation() {
        if (!readinessModel) {
            recommendedSource = ({});
            return;
        }
        var ids = readinessModel.rankedSourceIds();
        recommendedSource = ids.length > 0 ? readinessModel.source(ids[0]) : ({});
    }

    function refreshCandidates() {
        if (!readinessModel || !goal) {
            candidates = [];
            return;
        }
        var rows = [];
        var ids = readinessModel.rankedSourceIds();
        for (var i = 0; i < ids.length; i++) {
            var source = readinessModel.source(ids[i]);
            if (sourceMatchesGoal(source)) rows.push(source);
        }
        candidates = rows;
    }

    function sourceMatchesGoal(source) {
        if (goal === "local") return source.sourceKindKey === "local_tool";
        if (source.sourceKindKey !== "provider") return false;
        if (goal === "usage") {
            return source.monitoringLevel === "actual_usage_spend"
                || source.monitoringLevel === "actual_key_usage"
                || source.monitoringLevel === "balance_connectivity";
        }
        return source.monitoringLevel === "gateway_aggregate"
            || source.monitoringLevel === "connectivity_only";
    }

    function setCredential(slot, value) {
        var next = Object.assign({}, credentialValues);
        next[slot] = value;
        credentialValues = next;
    }

    function credentialLabel(slot) {
        var labels = {
            "bedrock_access_key_id": qsTr("Access key ID"),
            "bedrock_secret_access_key": qsTr("Secret access key"),
            "azure_openai_api_key": qsTr("API key")
        };
        return labels[slot] || qsTr("API key");
    }

    function hasStoredCredential(slot) {
        return secretStore && secretStore.walletOpen && secretStore.hasKey(slot);
    }

    function saveAndVerify() {
        refreshSelectedSource();
        statusMessage = "";
        statusError = false;
        var credentialsSaved = false;

        if (!selectedSourceId || !selectedSource.stableId) {
            fail(qsTr("Choose a source before continuing."));
            return false;
        }
        if (!selectedSource.safeVerification) {
            fail(qsTr("This source does not provide a safe read-only verification."));
            return false;
        }

        if (selectedSource.sourceKindKey === "provider") {
            if (requiredCredentialSlots.length > 0 && (!secretStore || !secretStore.walletOpen)) {
                fail(qsTr("KDE Wallet is not open. Unlock it and try again."));
                return false;
            }
            var endpoint = customEndpoint.trim();
            var hasSavedEndpoint = sourceApi.hasGuidedSourceEndpoint
                                 && sourceApi.hasGuidedSourceEndpoint(selectedSourceId);
            if (needsCustomEndpoint && !endpoint && !hasSavedEndpoint) {
                fail(qsTr("Enter the required endpoint URL before continuing."));
                return false;
            }
            if (endpoint && !(endpoint.startsWith("https://") || endpoint.startsWith("http://"))) {
                fail(qsTr("Enter a valid HTTP or HTTPS endpoint URL."));
                return false;
            }
            for (var i = 0; i < requiredCredentialSlots.length; i++) {
                var slot = requiredCredentialSlots[i];
                var value = (credentialValues[slot] || "").trim();
                if (!value && !hasStoredCredential(slot)) {
                    fail(qsTr("Enter the required credential before continuing."));
                    return false;
                }
                if (value) secretChanges.stageStore(slot, value);
            }
            var result = secretChanges.commit();
            if (!result.ok) {
                fail(result.message === "wallet-not-open"
                     ? qsTr("KDE Wallet is not open. Unlock it and try again.")
                     : qsTr("The credential could not be saved. It remains ready for retry."));
                return false;
            }
            credentialsSaved = result.appliedKeys.length > 0;
            if (endpoint && !sourceApi.setGuidedSourceEndpoint(selectedSourceId, endpoint)) {
                fail(qsTr("The endpoint could not be saved."));
                return false;
            }
        }

        if (!sourceApi.setGuidedSourceEnabled(selectedSourceId, true)) {
            fail(qsTr("The source could not be enabled."));
            return false;
        }

        goTo(verificationStep);
        statusMessage = selectedSource.sourceKindKey === "provider"
            ? (credentialsSaved
               ? qsTr("Credentials saved securely in KDE Wallet. Running a read-only verification…")
               : qsTr("Using the saved credential for a read-only verification…"))
            : qsTr("Checking the detected tool's local activity path…");
        verificationTimeout.restart();
        Qt.callLater(function() {
            controller.refreshSelectedSource();
            var started = sourceApi.verifyGuidedSource(selectedSourceId);
            controller.refreshSelectedSource();
            controller.evaluateVerification();
            if (!started && controller.step === controller.verificationStep
                    && controller.selectedSource.readinessStateKey !== "verifying") {
                controller.fail(controller.selectedSource.nextActionText
                                || qsTr("Verification could not start. Review the source configuration."));
            }
        });
        return true;
    }

    function evaluateVerification() {
        if (step !== verificationStep || !selectedSource.readinessStateKey) return;
        var state = selectedSource.readinessStateKey;
        if (state === "reporting_actual" || state === "reporting_estimate"
                || state === "connected_connectivity_only") {
            verificationTimeout.stop();
            resultQuality = qualityLabel(selectedSource);
            resultSummary = qualitySummary(selectedSource);
            configuration.setupWizardCompleted = true;
            configuration.setupWizardDismissed = false;
            configuration.setupWizardInProgress = true;
            goTo(resultStep);
            statusError = false;
            statusMessage = "";
        } else if (state === "failed" || state === "degraded"
                   || state === "needs_configuration" || state === "unavailable_locally") {
            verificationTimeout.stop();
            fail(selectedSource.nextActionText || qsTr("Verification failed. Review the source and try again."));
        }
    }

    function retryVerification() {
        goTo(configureStep);
        saveAndVerify();
    }

    function finish() {
        configuration.setupWizardCompleted = true;
        configuration.setupWizardDismissed = false;
        configuration.setupWizardInProgress = false;
        finished();
    }

    function fail(message) {
        statusError = true;
        statusMessage = message;
    }

    function monitoringLevelLabel(source) {
        var labels = {
            "actual_usage_spend": qsTr("Actual usage and provider-reported spend"),
            "actual_key_usage": qsTr("Actual key usage"),
            "gateway_aggregate": qsTr("Gateway-reported usage"),
            "balance_connectivity": qsTr("Balance and connectivity"),
            "connectivity_only": qsTr("Connectivity only"),
            "local_activity_estimate": qsTr("Local activity estimate")
        };
        return labels[source.monitoringLevel] || qsTr("Source status");
    }

    function qualityLabel(source) {
        if (source.sourceKindKey === "local_tool") return qsTr("Local activity estimate");
        var labels = {
            "actual_usage_spend": qsTr("Actual usage and spend"),
            "actual_key_usage": qsTr("Actual key usage"),
            "gateway_aggregate": qsTr("Gateway-reported usage"),
            "balance_connectivity": qsTr("Provider-reported balance"),
            "connectivity_only": qsTr("Connectivity only")
        };
        return labels[source.monitoringLevel] || qsTr("Verified source");
    }

    function qualitySummary(source) {
        if (source.sourceKindKey === "local_tool")
            return qsTr("The tool was detected and its local activity path was checked. Values remain local estimates unless an authenticated source reports a quota window.");
        if (source.monitoringLevel === "connectivity_only")
            return qsTr("The credential and endpoint worked. This provider does not expose usage or billing through the monitored endpoint.");
        if (source.monitoringLevel === "balance_connectivity")
            return qsTr("The provider returned its balance endpoint. A balance is not token usage or spend history.");
        if (source.monitoringLevel === "gateway_aggregate")
            return qsTr("The gateway returned aggregate usage or spend. The values come from the gateway, not directly from an upstream provider.");
        return qsTr("The source returned supported usage or spend data through its read-only reporting endpoint.");
    }

    Component.onCompleted: initialize()
}
