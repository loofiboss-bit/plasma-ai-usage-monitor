import QtQuick
import QtTest
import "../../../../package/contents/ui" as AppUi

TestCase {
    id: testCase
    name: "ProviderSettingsController"

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
        property string cfg_openaiProjectId: ""
        property string cfg_azureDeploymentId: ""
        property string cfg_bedrockRegion: "us-east-1"
        property string cfg_googleTier: "free"
        property string cfg_googleveoTier: "paid"
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
            monitoringLevel: level,
            enabledConfigKey: key + "Enabled",
            modelConfigKey: key + "Model",
            customBaseUrlConfigKey: key + "CustomBaseUrl"
        };
    }

    AppUi.ProviderSettingsController {
        id: controller
        descriptors: testCase.descriptors
        configuration: fakeConfiguration
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
        controller.searchText = "";
        controller.filterKey = "all";
        controller.selectedSourceId = "openai";
        controller.takeSnapshot();
    }

    function test_sourcesAreGroupedByUsefulDataFirst() {
        compare(controller.visibleSources.length, 5);
        compare(controller.visibleSources[0].configKey, "anthropic");
        compare(controller.visibleSources[1].configKey, "openai");
        compare(controller.visibleSources[2].configKey, "codex-cli");
        compare(controller.visibleSources[3].configKey, "litellm");
        compare(controller.visibleSources[4].configKey, "deepseek");
        compare(controller.visibleSources[0].categoryLabel, "Usage & spend");
    }

    function test_searchAndFilterPreserveSelection() {
        controller.selectedSourceId = "litellm";
        controller.searchText = "anthropic";
        compare(controller.visibleSources.length, 1);
        compare(controller.selectedSourceId, "litellm");

        controller.searchText = "";
        controller.filterKey = "usage";
        compare(controller.visibleSources.length, 2);
        compare(controller.selectedSourceId, "litellm");

        controller.filterKey = "all";
        compare(controller.selectedSource.configKey, "litellm");
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
        controller.setValue("advancedSettingsMode", true);
        verify(controller.dirty);

        controller.discardChanges();
        verify(!controller.dirty);
        verify(!fakeConfiguration.cfg_openaiEnabled);
        compare(fakeConfiguration.cfg_openaiModel, "gpt-default");
        verify(!fakeConfiguration.cfg_advancedSettingsMode);
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
}
