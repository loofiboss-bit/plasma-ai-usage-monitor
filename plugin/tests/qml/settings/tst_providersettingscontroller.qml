import QtQuick
import QtTest
import "../../../../package/contents/ui" as AppUi

TestCase {
    id: testCase
    name: "ProviderSettingsController"
    when: windowShown
    visible: true
    property alias settingsController: controller

    QtObject {
        id: fakeConfiguration
        property bool cfg_advancedSettingsMode: false
        property bool cfg_openaiEnabled: false
        property string cfg_openaiModel: "gpt-default"
        property string cfg_openaiCustomBaseUrl: ""
        property bool cfg_litellmEnabled: false
        property string cfg_litellmModel: "gateway-default"
        property string cfg_litellmCustomBaseUrl: ""
        property bool cfg_deepseekEnabled: false
        property string cfg_deepseekModel: "deepseek-default"
        property string cfg_deepseekCustomBaseUrl: ""
        property bool cfg_anthropicEnabled: false
        property string cfg_anthropicModel: "claude-default"
        property string cfg_anthropicCustomBaseUrl: ""
        property bool cfg_codexEnabled: false
        property bool cfg_claudeCodeEnabled: false
        property string cfg_openaiProjectId: ""
        property string cfg_azureDeploymentId: ""
        property string cfg_bedrockRegion: "us-east-1"
        property string cfg_googleTier: "free"
        property string cfg_googleveoTier: "paid"
        property bool cfg_browserSyncEnabled: false
        property int cfg_browserSyncBrowser: 0
        property int cfg_browserSyncInterval: 300
        property string cfg_browserSyncProfile: ""
        property bool cfg_copilotEnabled: true
        property string cfg_copilotPlan: "business-plan"
        property int cfg_copilotPlanIndex: 1
        property string cfg_copilotBillingMode: "auto"
        property bool cfg_copilotNotifications: false
        property int cfg_copilotResetDay: 1
        property int cfg_copilotCustomLimit: 1000
    }

    QtObject {
        id: fakeSourcePage
        property bool advancedMode: false
        property bool walletOpen: false
        property bool hasUnsavedChanges: controller.dirty
        property int secretCommitRevision: 0
        property var browserProfileModel: ["Auto (Default Profile)", "Work profile"]
        property var browserTestResults: ({})
        property int profileReloadCount: 0
        property int browserTestCount: 0
        property int verificationRequestCount: 0
        property string verificationSourceId: ""
        property string verificationState: ""
        property string verificationMessage: ""
        property string verificationTimestamp: ""

        function optionalSecretPlaceholder() { return "Optional"; }
        function hasStoredOrPendingSecret() { return false; }
        function sourceDetectedPlan() { return "Detected automatically"; }
        function sourceConnectionState() { return "Connected"; }
        function sourceLastSuccessfulRefresh() { return "Unavailable"; }
        function sourceQuotaStatus() { return "Unavailable"; }
        function requestVerification() {
            if (!hasUnsavedChanges) verificationRequestCount++;
        }
        function stageSecret() {}
        function removeSecret() {}
        function refreshSourceQuota() {}
        function reloadBrowserProfiles() { profileReloadCount++; }
        function testBrowserSession() {
            if (!hasUnsavedChanges) browserTestCount++;
        }
        function syncStatusColor() { return "gray"; }
        function catalogModelIds() { return []; }
        function isInvalidUrl() { return false; }
    }

    property var descriptors: [
        descriptor("anthropic", "Anthropic", "actual_usage_spend"),
        descriptor("deepseek", "DeepSeek", "balance_connectivity"),
        descriptor("litellm", "LiteLLM Proxy", "gateway_aggregate"),
        descriptor("openai", "OpenAI", "actual_usage_spend"),
        {
            configKey: "codex-cli", name: "Codex CLI", sourceKind: "local_tool",
            monitoringLevel: "local_activity_estimate", enabledConfigKey: "codexEnabled",
            modelConfigKey: "", customBaseUrlConfigKey: ""
        }
    ]

    function descriptor(key, name, level) {
        return {
            configKey: key,
            name: name,
            sourceKind: "api_service",
            monitoringLevel: level,
            enabledConfigKey: key + "Enabled",
            modelConfigKey: key + "Model",
            customBaseUrlConfigKey: key + "CustomBaseUrl",
            auth: { scheme: "none", credentialSlots: [] },
            safeRefresh: { method: "GET", paths: [] }
        };
    }

    AppUi.ProviderSettingsController {
        id: controller
        descriptors: testCase.descriptors
        configuration: fakeConfiguration
    }

    Component {
        id: sourceListComponent
        AppUi.ProviderSourceList {
            controller: testCase.settingsController
            width: 420
            height: 600
        }
    }

    Component {
        id: browserSyncComponent
        AppUi.BrowserSyncSettings {
            objectName: "browserSyncSettings"
            width: 620
            height: 760
            expanded: true
            controller: testCase.settingsController
            configPage: fakeSourcePage
        }
    }

    Component {
        id: apiDetailsComponent
        AppUi.ProviderSourceDetails {
            objectName: "apiSourceDetails"
            width: 720
            height: 900
            descriptor: testCase.descriptor("openai", "OpenAI", "actual_usage_spend")
            controller: testCase.settingsController
            configPage: fakeSourcePage
        }
    }

    property var copilotDescriptor: ({
        configKey: "github-copilot",
        name: "GitHub Copilot",
        sourceKind: "ai_tool",
        monitoringLevel: "actual_quota",
        enabledConfigKey: "copilotEnabled",
        modelConfigKey: "copilotPlan",
        legacyPlanConfigKey: "copilotPlanIndex",
        billingModeConfigKey: "copilotBillingMode",
        installed: true,
        notificationsConfigKey: "copilotNotifications",
        resetDayConfigKey: "copilotResetDay",
        customLimitConfigKey: "copilotCustomLimit",
        monitor: {
            availablePlans: function() { return ["Individual", "Business"]; },
            planIdForLabel: function(label) {
                return label === "Business" ? "business-plan" : "individual-plan";
            }
        }
    })

    Component {
        id: toolDetailsComponent
        AppUi.ConfigSubscriptions {
            objectName: "toolSourceDetails"
            width: 500
            height: 900
            descriptor: testCase.copilotDescriptor
            controller: testCase.settingsController
            configPage: fakeSourcePage
        }
    }

    function init() {
        testCase.descriptors = [
            descriptor("anthropic", "Anthropic", "actual_usage_spend"),
            descriptor("deepseek", "DeepSeek", "balance_connectivity"),
            descriptor("litellm", "LiteLLM Proxy", "gateway_aggregate"),
            descriptor("openai", "OpenAI", "actual_usage_spend"),
            {
                configKey: "codex-cli", name: "Codex CLI", sourceKind: "local_tool",
                monitoringLevel: "local_activity_estimate", enabledConfigKey: "codexEnabled",
                modelConfigKey: "", customBaseUrlConfigKey: ""
            }
        ];
        fakeConfiguration.cfg_advancedSettingsMode = false;
        fakeConfiguration.cfg_openaiEnabled = false;
        fakeConfiguration.cfg_openaiModel = "gpt-default";
        fakeConfiguration.cfg_openaiCustomBaseUrl = "";
        fakeConfiguration.cfg_litellmEnabled = false;
        fakeConfiguration.cfg_litellmModel = "gateway-default";
        fakeConfiguration.cfg_litellmCustomBaseUrl = "";
        fakeConfiguration.cfg_deepseekEnabled = false;
        fakeConfiguration.cfg_deepseekModel = "deepseek-default";
        fakeConfiguration.cfg_deepseekCustomBaseUrl = "";
        fakeConfiguration.cfg_anthropicEnabled = false;
        fakeConfiguration.cfg_anthropicModel = "claude-default";
        fakeConfiguration.cfg_anthropicCustomBaseUrl = "";
        fakeConfiguration.cfg_codexEnabled = false;
        fakeConfiguration.cfg_claudeCodeEnabled = false;
        fakeConfiguration.cfg_browserSyncEnabled = false;
        fakeConfiguration.cfg_browserSyncBrowser = 0;
        fakeConfiguration.cfg_browserSyncInterval = 300;
        fakeConfiguration.cfg_browserSyncProfile = "";
        fakeConfiguration.cfg_copilotEnabled = true;
        fakeConfiguration.cfg_copilotPlan = "business-plan";
        fakeConfiguration.cfg_copilotPlanIndex = 1;
        fakeConfiguration.cfg_copilotBillingMode = "auto";
        fakeConfiguration.cfg_copilotNotifications = false;
        fakeConfiguration.cfg_copilotResetDay = 1;
        fakeConfiguration.cfg_copilotCustomLimit = 1000;
        controller.searchText = "";
        controller.filterKey = "all";
        controller.selectedSourceId = "openai";
        controller.takeSnapshot();
    }

    function test_sourcesAreGroupedThenSortedByName() {
        compare(controller.visibleSources.length, 5);
        compare(controller.visibleSources[0].configKey, "codex-cli");
        compare(controller.visibleSources[1].configKey, "anthropic");
        compare(controller.visibleSources[2].configKey, "deepseek");
        compare(controller.visibleSources[3].configKey, "litellm");
        compare(controller.visibleSources[4].configKey, "openai");
        compare(controller.visibleSources[0].categoryLabel, "AI tools");
    }

    function test_searchAndFilterPreserveSelection() {
        controller.selectedSourceId = "litellm";
        controller.searchText = "anthropic";
        compare(controller.visibleSources.length, 1);
        compare(controller.selectedSourceId, "anthropic");

        controller.searchText = "";
        controller.filterKey = "usage";
        compare(controller.visibleSources.length, 2);
        compare(controller.selectedSourceId, "anthropic");

        controller.filterKey = "all";
        compare(controller.selectedSource.configKey, "anthropic");
    }

    function test_searchWithNoMatchesClearsSelection() {
        controller.selectedSourceId = "openai";
        controller.searchText = "does-not-exist";
        compare(controller.visibleSources.length, 0);
        compare(controller.selectedSourceId, "");
    }

    function test_stableToolDescriptorsRemainSelectableWhenNotInstalled() {
        var tools = ["claude-code", "codex-cli", "github-copilot", "cursor", "windsurf", "jetbrains-ai", "google-antigravity"];
        var rows = [];
        for (var i = 0; i < tools.length; ++i) {
            rows.push({
                configKey: tools[i], name: tools[i], sourceKind: "ai_tool",
                monitoringLevel: tools[i] === "google-antigravity" ? "actual_quota" : "local_activity_estimate",
                enabledConfigKey: "codexEnabled", modelConfigKey: "", customBaseUrlConfigKey: "",
                installed: false
            });
        }
        testCase.descriptors = rows;
        compare(controller.visibleSources.length, 7);
        controller.selectedSourceId = "github-copilot";
        compare(controller.selectedSource.configKey, "github-copilot");
        compare(controller.visibleSources[0].categoryLabel, "AI tools");
    }

    function test_sourceFiltersSelectFirstVisibleAndDetectedOnlyShowsTools() {
        controller.selectedSourceId = "deepseek";
        controller.filterKey = "enabled";
        compare(controller.visibleSources.length, 0);
        compare(controller.selectedSourceId, "");

        controller.filterKey = "detected";
        compare(controller.visibleSources.length, 0);
        testCase.descriptors = testCase.descriptors.concat([{
            configKey: "claude-code", name: "Claude Code", sourceKind: "ai_tool",
            monitoringLevel: "local_activity_estimate", enabledConfigKey: "codexEnabled",
            modelConfigKey: "", customBaseUrlConfigKey: "", installed: true
        }]);
        compare(controller.visibleSources.length, 1);
        compare(controller.selectedSourceId, "claude-code");
    }

    function test_activeToolsSortBeforeDetectedAndOtherTools() {
        testCase.descriptors = [
            {configKey: "zeta-tool", name: "Zeta Tool", sourceKind: "ai_tool",
                monitoringLevel: "local_activity_estimate", enabledConfigKey: "codexEnabled",
                installed: false},
            {configKey: "alpha-tool", name: "Alpha Tool", sourceKind: "ai_tool",
                monitoringLevel: "local_activity_estimate", enabledConfigKey: "claudeCodeEnabled",
                installed: true}
        ];
        fakeConfiguration.cfg_codexEnabled = true;
        fakeConfiguration.cfg_claudeCodeEnabled = false;
        controller.filterKey = "all";
        compare(controller.visibleSources[0].configKey, "zeta-tool");
        fakeConfiguration.cfg_codexEnabled = false;
        tryCompare(controller.visibleSources, "length", 2);
        compare(controller.visibleSources[0].configKey, "alpha-tool");
    }

    function test_apiEnabledSourcesSortBeforeMonitoringCategoryAndName() {
        testCase.descriptors = [
            descriptor("anthropic", "Anthropic", "actual_usage_spend"),
            descriptor("deepseek", "DeepSeek", "balance_connectivity"),
            descriptor("litellm", "LiteLLM Proxy", "gateway_aggregate")
        ];
        fakeConfiguration.cfg_anthropicEnabled = false;
        fakeConfiguration.cfg_deepseekEnabled = true;
        fakeConfiguration.cfg_litellmEnabled = true;

        compare(controller.visibleSources.length, 3);
        compare(controller.visibleSources[0].configKey, "deepseek");
        compare(controller.visibleSources[1].configKey, "litellm");
        compare(controller.visibleSources[2].configKey, "anthropic");
    }

    function test_applyAcceptsCurrentConfigurationAsBaseline() {
        controller.setValue("openaiEnabled", true);
        controller.setValue("openaiModel", "gpt-custom");
        verify(controller.dirty);
        controller.acceptChanges();
        verify(!controller.dirty);
        verify(fakeConfiguration.cfg_openaiEnabled);
        compare(fakeConfiguration.cfg_openaiModel, "gpt-custom");
    }

    function test_cancelRestoresEveryStagedConfigurationValue() {
        controller.setValue("openaiEnabled", true);
        controller.setValue("openaiModel", "gpt-custom");
        verify(controller.dirty);

        controller.discardChanges();
        verify(!controller.dirty);
        verify(!fakeConfiguration.cfg_openaiEnabled);
        compare(fakeConfiguration.cfg_openaiModel, "gpt-default");
    }

    function test_lateLocalDetectionExtendsBaselineWithoutDirtyingPage() {
        var localSource = testCase.descriptors[4];
        testCase.descriptors = testCase.descriptors.slice(0, 4);
        controller.takeSnapshot();
        verify(!controller.dirty);

        testCase.descriptors = testCase.descriptors.concat([localSource]);
        tryCompare(controller.visibleSources, "length", 5);
        verify(!controller.dirty);

        controller.setValue("openaiEnabled", true);
        verify(controller.dirty);
        controller.discardChanges();
        verify(!fakeConfiguration.cfg_openaiEnabled);
    }

    function test_browserSyncDraftsParticipateInApplyAndCancel() {
        controller.setValue("browserSyncEnabled", true);
        controller.setValue("browserSyncBrowser", 2);
        controller.setValue("browserSyncProfile", "work-profile");
        verify(controller.dirty);
        controller.discardChanges();
        verify(!controller.dirty);
        verify(!fakeConfiguration.cfg_browserSyncEnabled);
        compare(fakeConfiguration.cfg_browserSyncBrowser, 0);
        compare(fakeConfiguration.cfg_browserSyncProfile, "");
    }

    function test_editsAcrossSourcesRemainStagedUntilApply() {
        controller.selectedSourceId = "openai";
        controller.setValue("openaiModel", "gpt-reviewed");
        controller.selectedSourceId = "anthropic";
        controller.setValue("anthropicModel", "claude-reviewed");
        controller.searchText = "anthropic";

        compare(controller.visibleSources.length, 1);
        verify(controller.dirty);
        compare(controller.value("openaiModel"), "gpt-reviewed");
        compare(controller.value("anthropicModel"), "claude-reviewed");
        compare(controller.baseline.openaiModel, "gpt-default");
        compare(controller.baseline.anthropicModel, "claude-default");

        controller.acceptChanges();
        compare(controller.baseline.openaiModel, "gpt-reviewed");
        compare(controller.baseline.anthropicModel, "claude-reviewed");
        verify(!controller.dirty);
    }

    function test_toolPlanAndBillingEditsStayStagedUntilApplyOrCancel() {
        testCase.descriptors = [copilotDescriptor];
        controller.takeSnapshot();
        controller.selectedSourceId = "github-copilot";
        var details = createTemporaryObject(toolDetailsComponent, testCase);
        verify(details !== null);

        var plan = findChild(details, "sourcePlanSelector");
        var billing = findChild(details, "sourceBillingModeSelector");
        var quotaWarning = findChild(details, "sourceQuotaWarningSwitch");
        var advanced = findChild(details, "sourceAdvancedSettings");
        var resetDay = findChild(details, "monthlyResetDaySelector");
        var customLimit = findChild(details, "customQuotaLimitSelector");
        verify(plan !== null);
        verify(billing !== null);
        verify(quotaWarning !== null);
        verify(advanced !== null);
        verify(resetDay !== null);
        verify(customLimit !== null);
        verify(billing.visible);
        verify(quotaWarning.visible);
        verify(!advanced.visible);
        verify(!resetDay.visible);
        verify(!customLimit.visible);
        compare(plan.currentIndex, 1);

        plan.currentIndex = 0;
        plan.activated(0);
        billing.currentIndex = 2;
        billing.activated(2);
        verify(controller.dirty);
        compare(controller.value("copilotPlan"), "individual-plan");
        compare(controller.value("copilotPlanIndex"), 0);
        compare(controller.value("copilotBillingMode"), "ai_credits_usage_based");
        compare(controller.baseline.copilotPlan, "business-plan");

        controller.discardChanges();
        verify(!controller.dirty);
        compare(controller.value("copilotPlan"), "business-plan");
        compare(controller.value("copilotBillingMode"), "auto");

        plan.currentIndex = 0;
        plan.activated(0);
        controller.acceptChanges();
        verify(!controller.dirty);
        compare(controller.baseline.copilotPlan, "individual-plan");
    }

    function test_enabledFilterMovesSelectionAfterActualSourceToggle() {
        testCase.descriptors = [copilotDescriptor,
            descriptor("openai", "OpenAI", "actual_usage_spend")];
        fakeConfiguration.cfg_copilotEnabled = true;
        fakeConfiguration.cfg_openaiEnabled = true;
        controller.takeSnapshot();
        controller.filterKey = "enabled";
        controller.selectedSourceId = "github-copilot";

        var details = createTemporaryObject(toolDetailsComponent, testCase);
        verify(details !== null);
        var enableSwitch = findChild(details, "sourceEnabledSwitch");
        verify(enableSwitch !== null);
        verify(enableSwitch.checked);
        mouseClick(enableSwitch, enableSwitch.width / 2, enableSwitch.height / 2);

        tryCompare(controller, "selectedSourceId", "openai");
        compare(controller.visibleSources.length, 1);
        compare(controller.visibleSources[0].configKey, "openai");
        verify(!fakeConfiguration.cfg_copilotEnabled);
        verify(controller.dirty);
    }

    function test_browserSyncUiStagesEveryControlAndBlocksVerificationUntilApply() {
        fakeConfiguration.cfg_openaiEnabled = true;
        controller.acceptChanges();
        fakeSourcePage.verificationRequestCount = 0;
        fakeSourcePage.browserTestCount = 0;
        fakeSourcePage.profileReloadCount = 0;

        var sync = createTemporaryObject(browserSyncComponent, testCase);
        var details = createTemporaryObject(apiDetailsComponent, testCase);
        verify(sync !== null);
        verify(details !== null);

        var enabled = findChild(sync, "browserSyncEnabledSwitch");
        var browser = findChild(sync, "browserSyncBrowserSelector");
        var profile = findChild(sync, "browserSyncProfileSelector");
        var interval = findChild(sync, "browserSyncIntervalSelector");
        var claudeTest = findChild(sync, "browserSyncTestClaudeButton");
        var verifyButton = findChild(details, "sourceVerificationButton");
        verify(enabled !== null);
        verify(browser !== null);
        verify(profile !== null);
        verify(interval !== null);
        verify(claudeTest !== null);
        verify(verifyButton !== null);

        mouseClick(enabled, enabled.width / 2, enabled.height / 2);
        tryCompare(controller, "dirty", true);
        controller.acceptChanges();
        verify(!controller.dirty);
        browser.currentIndex = 1;
        browser.activated(1);
        verify(controller.dirty);
        controller.discardChanges();
        verify(!controller.dirty);
        profile.currentIndex = 1;
        profile.activated(1);
        verify(controller.dirty);
        controller.discardChanges();
        verify(!controller.dirty);
        interval.value = 600;
        interval.valueModified();
        verify(controller.dirty);
        browser.currentIndex = 1;
        browser.activated(1);
        profile.currentIndex = 1;
        profile.activated(1);

        verify(fakeSourcePage.hasUnsavedChanges);
        verify(!claudeTest.enabled);
        verify(!verifyButton.enabled);
        mouseClick(claudeTest, claudeTest.width / 2, claudeTest.height / 2);
        mouseClick(verifyButton, verifyButton.width / 2, verifyButton.height / 2);
        compare(fakeSourcePage.browserTestCount, 0);
        compare(fakeSourcePage.verificationRequestCount, 0);
        compare(fakeConfiguration.cfg_browserSyncBrowser, 1);
        compare(fakeConfiguration.cfg_browserSyncProfile, "Work profile");
        compare(fakeConfiguration.cfg_browserSyncInterval, 600);

        controller.discardChanges();
        verify(!fakeSourcePage.hasUnsavedChanges);
        verify(fakeConfiguration.cfg_browserSyncEnabled);
        compare(fakeConfiguration.cfg_browserSyncBrowser, 0);
        compare(fakeConfiguration.cfg_browserSyncProfile, "");
        compare(fakeConfiguration.cfg_browserSyncInterval, 300);
        verify(verifyButton.enabled);
    }

    function test_sourceListKeyboardSelectionAndFocusRestore() {
        var list = createTemporaryObject(sourceListComponent, testCase);
        verify(list !== null);
        controller.filterKey = "all";
        controller.searchText = "";
        controller.selectedSourceId = "anthropic";
        var view = findChild(list, "providerSourceList");
        verify(view !== null);
        view.forceActiveFocus();
        tryCompare(view, "activeFocus", true);
        keyClick(Qt.Key_Down);
        compare(controller.selectedSourceId, "deepseek");

        controller.selectedSourceId = "anthropic";
        list.focusSelectedSource();
        tryCompare(view.currentItem, "activeFocus", true);
    }
}
