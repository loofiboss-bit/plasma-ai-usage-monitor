import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: providersPage
    signal configurationChanged()

    ProviderCatalog { id: descriptorCatalog }

    property bool advancedMode: plasmoid.configuration.advancedSettingsMode
    property string providerSearch: ""
    property string capabilityFilter: "all"

    function providerMatches(descriptor) {
        var needle = providerSearch.trim().toLowerCase();
        var textMatch = needle.length === 0
            || descriptor.name.toLowerCase().indexOf(needle) >= 0
            || descriptor.configKey.toLowerCase().indexOf(needle) >= 0;
        if (!textMatch) return false;
        if (capabilityFilter === "all") return true;
        if (capabilityFilter === "actual") return descriptor.monitoringLevel.indexOf("actual") === 0;
        if (capabilityFilter === "gateway") return descriptor.monitoringLevel === "gateway_aggregate";
        if (capabilityFilter === "balance") return descriptor.monitoringLevel === "balance_connectivity";
        return descriptor.monitoringLevel === "connectivity_only";
    }

    function monitoringLabel(level) {
        if (level === "actual_usage_spend") return i18n("Actual account usage/spend");
        if (level === "actual_key_usage") return i18n("Actual key usage/balance");
        if (level === "gateway_aggregate") return i18n("Gateway aggregate");
        if (level === "balance_connectivity") return i18n("Balance/connectivity");
        return i18n("Connectivity/model discovery only");
    }

    function scheduledCalls(safeRefresh) {
        var paths = safeRefresh.paths || [safeRefresh.path || ""];
        var calls = [];
        for (var i = 0; i < paths.length; i++) {
            if (paths[i]) calls.push((safeRefresh.method || "GET") + " " + paths[i]);
        }
        return calls.join("; ");
    }

    onAdvancedModeChanged: {
        plasmoid.configuration.advancedSettingsMode = advancedMode
    }

    // URL validation helper
    function isInvalidUrl(url) {
        if (url.length === 0) return false;
        var lower = url.toLowerCase();
        return !lower.startsWith("https://")
            && !lower.startsWith("http://localhost")
            && !lower.startsWith("http://127.0.0.1")
            && !lower.startsWith("http://[::1]");
    }

    function stageSecret(key, value) {
        if (value.length > 0) secretChanges.stageStore(key, value);
        else secretChanges.stageRemove(key);
        secretStatusMessage = "";
        secretStatusError = false;
    }

    function clearSecret(key, field) {
        field.text = "";
        secretChanges.stageRemove(key);
        secretStatusMessage = "";
        secretStatusError = false;
    }

    // Keep every visible model picker aligned with Provider Catalog v5.
    // The fields stay editable so users can enter a newly released or custom
    // gateway model before the next catalog refresh.
    function catalogModelIds(providerKey) {
        var rows = ProviderPricingCatalog.selectableModelsForProvider(providerKey);
        var ids = [];
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].id) ids.push(rows[i].id);
        }
        return ids;
    }

    property alias cfg_openaiEnabled: openaiSwitch.checked
    property alias cfg_openaiModel: openaiModelField.text
    property alias cfg_openaiProjectId: openaiProjectField.text
    property alias cfg_openaiCustomBaseUrl: openaiBaseUrlField.text

    property alias cfg_azureEnabled: azureSwitch.checked
    property alias cfg_azureModel: azureModelField.text
    property alias cfg_azureDeploymentId: azureDeploymentField.text
    property alias cfg_azureCustomBaseUrl: azureBaseUrlField.text

    property alias cfg_bedrockEnabled: bedrockSwitch.checked
    property alias cfg_bedrockRegion: bedrockRegionField.text
    property alias cfg_bedrockModel: bedrockModelField.text
    property alias cfg_bedrockCustomBaseUrl: bedrockBaseUrlField.text

    property alias cfg_anthropicEnabled: anthropicSwitch.checked
    property alias cfg_anthropicModel: anthropicModelField.text
    property alias cfg_anthropicCustomBaseUrl: anthropicBaseUrlField.text

    property alias cfg_googleEnabled: googleSwitch.checked
    property alias cfg_googleModel: googleModelField.text
    property string cfg_googleTier: "free"
    property alias cfg_googleCustomBaseUrl: googleBaseUrlField.text

    property bool cfg_mistralEnabled: plasmoid.configuration.mistralEnabled
    property string cfg_mistralModel: plasmoid.configuration.mistralModel
    property string cfg_mistralCustomBaseUrl: plasmoid.configuration.mistralCustomBaseUrl

    property bool cfg_deepseekEnabled: plasmoid.configuration.deepseekEnabled
    property string cfg_deepseekModel: plasmoid.configuration.deepseekModel
    property string cfg_deepseekCustomBaseUrl: plasmoid.configuration.deepseekCustomBaseUrl

    property bool cfg_groqEnabled: plasmoid.configuration.groqEnabled
    property string cfg_groqModel: plasmoid.configuration.groqModel
    property string cfg_groqCustomBaseUrl: plasmoid.configuration.groqCustomBaseUrl

    property bool cfg_xaiEnabled: plasmoid.configuration.xaiEnabled
    property string cfg_xaiModel: plasmoid.configuration.xaiModel
    property string cfg_xaiCustomBaseUrl: plasmoid.configuration.xaiCustomBaseUrl

    property bool cfg_ollamaEnabled: plasmoid.configuration.ollamaEnabled
    property string cfg_ollamaModel: plasmoid.configuration.ollamaModel
    property string cfg_ollamaCustomBaseUrl: plasmoid.configuration.ollamaCustomBaseUrl

    property bool cfg_openrouterEnabled: plasmoid.configuration.openrouterEnabled
    property string cfg_openrouterModel: plasmoid.configuration.openrouterModel
    property string cfg_openrouterCustomBaseUrl: plasmoid.configuration.openrouterCustomBaseUrl

    property bool cfg_togetherEnabled: plasmoid.configuration.togetherEnabled
    property string cfg_togetherModel: plasmoid.configuration.togetherModel
    property string cfg_togetherCustomBaseUrl: plasmoid.configuration.togetherCustomBaseUrl

    property bool cfg_cohereEnabled: plasmoid.configuration.cohereEnabled
    property string cfg_cohereModel: plasmoid.configuration.cohereModel
    property string cfg_cohereCustomBaseUrl: plasmoid.configuration.cohereCustomBaseUrl

    property alias cfg_googleveoEnabled: googleveoSwitch.checked
    property alias cfg_googleveoModel: googleveoModelField.text
    property string cfg_googleveoTier: "paid"
    property alias cfg_googleveoCustomBaseUrl: googleveoBaseUrlField.text

    property bool cfg_litellmEnabled: plasmoid.configuration.litellmEnabled
    property string cfg_litellmModel: plasmoid.configuration.litellmModel
    property string cfg_litellmCustomBaseUrl: plasmoid.configuration.litellmCustomBaseUrl
    property bool cfg_cerebrasEnabled: plasmoid.configuration.cerebrasEnabled
    property string cfg_cerebrasModel: plasmoid.configuration.cerebrasModel
    property string cfg_cerebrasCustomBaseUrl: plasmoid.configuration.cerebrasCustomBaseUrl
    property bool cfg_fireworksEnabled: plasmoid.configuration.fireworksEnabled
    property string cfg_fireworksModel: plasmoid.configuration.fireworksModel
    property string cfg_fireworksCustomBaseUrl: plasmoid.configuration.fireworksCustomBaseUrl
    property bool cfg_perplexityEnabled: plasmoid.configuration.perplexityEnabled
    property string cfg_perplexityModel: plasmoid.configuration.perplexityModel
    property string cfg_perplexityCustomBaseUrl: plasmoid.configuration.perplexityCustomBaseUrl

    readonly property bool unsavedChanges: secretChanges.dirty
    readonly property bool walletOpen: secrets.walletOpen
    property string secretStatusMessage: ""
    property bool secretStatusError: false

    // ── KWallet Integration ──
    SecretsManager {
        id: secrets

        onWalletOpenChanged: {
            if (walletOpen && !secretChanges.dirty) {
                loadKeys();
            }
        }

        onKeyStored: function(provider) {
            console.log("Key stored for", provider);
        }

        onError: function(message) {
            console.warn("SecretsManager error:", message);
        }
    }

    SecretChangeSet {
        id: secretChanges
        store: secrets
    }

    function secretFields() {
        return [
            { name: "openai", field: openaiKeyField },
            { name: "anthropic", field: anthropicKeyField },
            { name: "google", field: googleKeyField },
            { name: "mistral", field: mistralSection.keyField },
            { name: "deepseek", field: deepseekSection.keyField },
            { name: "groq", field: groqSection.keyField },
            { name: "xai", field: xaiSection.keyField },
            { name: "ollama", field: ollamaSection.keyField },
            { name: "openrouter", field: openrouterSection.keyField },
            { name: "together", field: togetherSection.keyField },
            { name: "cohere", field: cohereSection.keyField },
            { name: "googleveo", field: googleveoKeyField },
            { name: "azure", field: azureKeyField },
            { name: "bedrock_access_key_id", field: bedrockAccessKeyField },
            { name: "bedrock_secret_access_key", field: bedrockSecretKeyField },
            { name: "bedrock_session_token", field: bedrockSessionTokenField },
            { name: "litellm", field: litellmSection.keyField },
            { name: "cerebras", field: cerebrasSection.keyField },
            { name: "fireworks", field: fireworksSection.keyField }
        ];
    }

    function refreshSecretFields(keys) {
        var providers = secretFields();
        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (keys && keys.indexOf(p.name) < 0) continue;
            if (secrets.hasKey(p.name)) {
                p.field.text = "********";
            } else {
                p.field.text = "";
            }
        }
    }

    function loadKeys() {
        refreshSecretFields(null);
    }

    function saveConfig() {
        var result = secretChanges.commit();
        secretStatusError = !result.ok;
        if (result.ok) {
            secretStatusMessage = result.appliedKeys.length > 0
                ? i18n("Credentials saved securely in KDE Wallet.") : "";
            if (result.appliedKeys.length > 0) {
                loadKeys();
            }
        } else if (result.message === "wallet-not-open") {
            secretStatusMessage = i18n("KDE Wallet is not open. Unlock it and retry Apply.");
        } else {
            secretStatusMessage = i18n("Some credentials could not be saved. Review the fields and retry Apply.");
            if (result.appliedKeys.length > 0) refreshSecretFields(result.appliedKeys);
        }
        if (!result.ok) {
            Qt.callLater(function() { providersPage.configurationChanged(); });
        }
    }

    Component.onCompleted: {
        if (secrets.walletOpen) {
            loadKeys();
        }
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        Kirigami.InlineMessage {
            visible: providersPage.secretStatusMessage.length > 0
            text: providersPage.secretStatusMessage
            type: providersPage.secretStatusError ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            Layout.fillWidth: true
        }
        
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Settings Mode")
        }
        
        QQC2.Switch {
            id: advancedModeSwitch
            Kirigami.FormData.label: i18n("Advanced Mode:")
            checked: providersPage.advancedMode
            onCheckedChanged: providersPage.advancedMode = checked
            QQC2.ToolTip.text: i18n("Show advanced configuration options like custom base URLs and specific tiers.")
            QQC2.ToolTip.visible: hovered
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Provider setup overview")
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Search:")
            placeholderText: i18n("Provider name")
            text: providersPage.providerSearch
            onTextChanged: providersPage.providerSearch = text
            Layout.fillWidth: true
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Monitoring level:")
            textRole: "text"
            valueRole: "value"
            model: [
                { text: i18n("All providers"), value: "all" },
                { text: i18n("Actual usage/spend"), value: "actual" },
                { text: i18n("Gateway/aggregate"), value: "gateway" },
                { text: i18n("Balance/credits"), value: "balance" },
                { text: i18n("Connectivity only"), value: "connectivity" }
            ]
            onActivated: providersPage.capabilityFilter = currentValue
            Layout.fillWidth: true
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Matches:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: descriptorCatalog.providers.filter(function(row) {
                    return providersPage.providerMatches(row);
                })

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true

                    QQC2.Switch {
                        checked: !!plasmoid.configuration[modelData.enabledConfigKey]
                        onToggled: plasmoid.configuration[modelData.enabledConfigKey] = checked
                        text: modelData.name
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: providersPage.monitoringLabel(modelData.monitoringLevel)
                              + " · " + providersPage.scheduledCalls(modelData.safeRefresh)
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
        

        // ══════════════════════════════════════════════
        // ── OpenAI ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("OpenAI")
        }

        QQC2.Switch {
            id: openaiSwitch
            Kirigami.FormData.label: i18n("Enable:")
            checked: plasmoid.configuration.openaiEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Admin API Key:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: openaiKeyField
                enabled: openaiSwitch.checked && secrets.walletOpen
                echoMode: openaiKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("sk-admin-...")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("openai", text)
            }

            QQC2.ToolButton {
                id: openaiKeyVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? i18n("Hide key") : i18n("Show key")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: secrets.walletOpen && openaiKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: providersPage.clearSecret("openai", openaiKeyField)
            }
        }

        QQC2.Label {
            visible: openaiSwitch.checked
            text: i18n("Requires an Admin API key for usage/costs endpoints")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.ComboBox {
            id: openaiModelField
            Kirigami.FormData.label: i18n("Model filter:")
            enabled: openaiSwitch.checked
            editable: true
            editText: plasmoid.configuration.openaiModel
            model: providersPage.catalogModelIds("openai")
            Layout.fillWidth: true
            onEditTextChanged: plasmoid.configuration.openaiModel = editText
            property alias text: openaiModelField.editText
            QQC2.ToolTip.text: i18n("Only show usage for this model. Leave empty to show all models.")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
        }

        QQC2.TextField {
            id: openaiProjectField
            Kirigami.FormData.label: i18n("Project ID (optional):")
            enabled: openaiSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.openaiProjectId
            placeholderText: "proj_..."
            Layout.fillWidth: true
        }

        QQC2.TextField {
            id: openaiBaseUrlField
            Kirigami.FormData.label: i18n("Custom base URL:")
            enabled: openaiSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.openaiCustomBaseUrl
            placeholderText: i18n("Leave empty for default")
            Layout.fillWidth: true
            QQC2.ToolTip.text: i18n("Override the API endpoint for proxies or self-hosted gateways. Must start with https://")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(openaiBaseUrlField.text)
            text: i18n("⚠ Use HTTPS, or HTTP only for a local loopback address")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(openaiBaseUrlField.text)
                     && openaiBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ External HTTP is blocked because it would expose API credentials.")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ══════════════════════════════════════════════
        // ── Azure OpenAI ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Azure OpenAI")
        }

        QQC2.Switch {
            id: azureSwitch
            Kirigami.FormData.label: i18n("Enable:")
            checked: plasmoid.configuration.azureEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("API Key:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: azureKeyField
                enabled: azureSwitch.checked && secrets.walletOpen
                echoMode: azureKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("Enter Azure OpenAI API key...")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("azure", text)
            }

            QQC2.ToolButton {
                id: azureKeyVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? i18n("Hide key") : i18n("Show key")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: secrets.walletOpen && azureKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: providersPage.clearSecret("azure", azureKeyField)
            }
        }

        QQC2.Label {
            visible: azureSwitch.checked
            text: i18n("Use your Azure OpenAI resource endpoint and deployment for monitoring")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.ComboBox {
            id: azureModelField
            Kirigami.FormData.label: i18n("Model filter:")
            enabled: azureSwitch.checked
            editable: true
            editText: plasmoid.configuration.azureModel
            model: providersPage.catalogModelIds("azure")
            Layout.fillWidth: true
            onEditTextChanged: plasmoid.configuration.azureModel = editText
            property alias text: azureModelField.editText
        }

        QQC2.TextField {
            id: azureDeploymentField
            Kirigami.FormData.label: i18n("Deployment ID:")
            enabled: azureSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.azureDeploymentId
            placeholderText: i18n("my-deployment")
            Layout.fillWidth: true
        }

        QQC2.TextField {
            id: azureBaseUrlField
            Kirigami.FormData.label: i18n("Endpoint base URL:")
            enabled: azureSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.azureCustomBaseUrl
            placeholderText: i18n("https://<resource>.openai.azure.com")
            Layout.fillWidth: true
            QQC2.ToolTip.text: i18n("Azure endpoint base URL. Must start with https://")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(azureBaseUrlField.text)
            text: i18n("⚠ Use HTTPS, or HTTP only for a local loopback address")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(azureBaseUrlField.text)
                     && azureBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ External HTTP is blocked because it would expose API credentials.")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ══════════════════════════════════════════════
        // ── AWS Bedrock ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("AWS Bedrock")
        }

        QQC2.Switch {
            id: bedrockSwitch
            Kirigami.FormData.label: i18n("Enable:")
            checked: plasmoid.configuration.bedrockEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Access key ID:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: bedrockAccessKeyField
                enabled: bedrockSwitch.checked && secrets.walletOpen
                echoMode: bedrockAccessKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AKIA...")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("bedrock_access_key_id", text)
            }

            QQC2.ToolButton {
                id: bedrockAccessKeyVisible
                checkable: true
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Secret access key:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: bedrockSecretKeyField
                enabled: bedrockSwitch.checked && secrets.walletOpen
                echoMode: bedrockSecretKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AWS secret access key")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("bedrock_secret_access_key", text)
            }

            QQC2.ToolButton {
                id: bedrockSecretKeyVisible
                checkable: true
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
            }
        }

        QQC2.TextField {
            id: bedrockSessionTokenField
            Kirigami.FormData.label: i18n("Session token:")
            enabled: bedrockSwitch.checked && secrets.walletOpen
            echoMode: TextInput.Password
            placeholderText: i18n("Optional for temporary credentials")
            Layout.fillWidth: true
            onTextEdited: providersPage.stageSecret("bedrock_session_token", text)
        }

        QQC2.TextField {
            id: bedrockRegionField
            Kirigami.FormData.label: i18n("Region:")
            enabled: bedrockSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.bedrockRegion
            placeholderText: "us-east-1"
            Layout.fillWidth: true
        }

        QQC2.ComboBox {
            id: bedrockModelField
            Kirigami.FormData.label: i18n("Model ID:")
            enabled: bedrockSwitch.checked
            editable: true
            editText: plasmoid.configuration.bedrockModel
            model: providersPage.catalogModelIds("bedrock")
            Layout.fillWidth: true
            onEditTextChanged: plasmoid.configuration.bedrockModel = editText
            property alias text: bedrockModelField.editText
        }

        QQC2.TextField {
            id: bedrockBaseUrlField
            Kirigami.FormData.label: i18n("Custom base URL:")
            enabled: bedrockSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.bedrockCustomBaseUrl
            placeholderText: i18n("Leave empty for AWS regional endpoint")
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: bedrockSwitch.checked
            text: i18n("Bedrock monitoring verifies AWS credentials and regional model availability. Cost remains estimated when AWS does not expose direct spend totals in-widget.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ══════════════════════════════════════════════
        // ── Anthropic ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Anthropic")
        }

        QQC2.Switch {
            id: anthropicSwitch
            Kirigami.FormData.label: i18n("Enable:")
            checked: plasmoid.configuration.anthropicEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("API Key:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: anthropicKeyField
                enabled: anthropicSwitch.checked && secrets.walletOpen
                echoMode: anthropicKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("sk-ant-...")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("anthropic", text)
            }

            QQC2.ToolButton {
                id: anthropicKeyVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? i18n("Hide key") : i18n("Show key")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: secrets.walletOpen && anthropicKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: providersPage.clearSecret("anthropic", anthropicKeyField)
            }
        }

        QQC2.Label {
            visible: anthropicSwitch.checked
            text: i18n("Shows rate limit status only (Anthropic has no usage API)")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.ComboBox {
            id: anthropicModelField
            Kirigami.FormData.label: i18n("Model:")
            enabled: anthropicSwitch.checked
            editable: true
            editText: plasmoid.configuration.anthropicModel
            model: providersPage.catalogModelIds("anthropic")
            onEditTextChanged: plasmoid.configuration.anthropicModel = editText
            property alias text: anthropicModelField.editText
        }

        QQC2.TextField {
            id: anthropicBaseUrlField
            Kirigami.FormData.label: i18n("Custom base URL:")
            enabled: anthropicSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.anthropicCustomBaseUrl
            placeholderText: i18n("Leave empty for default")
            Layout.fillWidth: true
            QQC2.ToolTip.text: i18n("Override the API endpoint for proxies or self-hosted gateways. Must start with https://")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(anthropicBaseUrlField.text)
            text: i18n("⚠ Use HTTPS, or HTTP only for a local loopback address")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(anthropicBaseUrlField.text)
                     && anthropicBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ External HTTP is blocked because it would expose API credentials.")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ══════════════════════════════════════════════
        // ── Google Gemini ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Google Gemini")
        }

        QQC2.Switch {
            id: googleSwitch
            Kirigami.FormData.label: i18n("Enable:")
            checked: plasmoid.configuration.googleEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("API Key:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: googleKeyField
                enabled: googleSwitch.checked && secrets.walletOpen
                echoMode: googleKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AIza...")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("google", text)
            }

            QQC2.ToolButton {
                id: googleKeyVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? i18n("Hide key") : i18n("Show key")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: secrets.walletOpen && googleKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: providersPage.clearSecret("google", googleKeyField)
            }
        }

        QQC2.Label {
            visible: googleSwitch.checked
            text: i18n("Uses read-only Gemini model discovery; published caps are not live remaining quota")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.ComboBox {
            id: googleModelField
            Kirigami.FormData.label: i18n("Model:")
            enabled: googleSwitch.checked
            editable: true
            editText: plasmoid.configuration.googleModel
            model: providersPage.catalogModelIds("google")
            onEditTextChanged: plasmoid.configuration.googleModel = editText
            property alias text: googleModelField.editText
        }

        QQC2.ComboBox {
            id: googleTierField
            Kirigami.FormData.label: i18n("Pricing tier:")
            enabled: googleSwitch.checked
            visible: providersPage.advancedMode
            model: [
                { text: i18n("Free"), value: "free" },
                { text: i18n("Paid (Pay-as-you-go)"), value: "paid" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: cfg_googleTier === "paid" ? 1 : 0
            onActivated: cfg_googleTier = currentValue
        }

        QQC2.Label {
            visible: googleSwitch.checked && googleTierField.currentIndex === 0
            text: i18n("Free tier has lower rate limits. Select Paid if you have billing enabled.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.TextField {
            id: googleBaseUrlField
            Kirigami.FormData.label: i18n("Custom base URL:")
            enabled: googleSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.googleCustomBaseUrl
            placeholderText: i18n("Leave empty for default")
            Layout.fillWidth: true
            QQC2.ToolTip.text: i18n("Override the API endpoint for proxies or self-hosted gateways. Must start with https://")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(googleBaseUrlField.text)
            text: i18n("⚠ Use HTTPS, or HTTP only for a local loopback address")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(googleBaseUrlField.text)
                     && googleBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ External HTTP is blocked because it would expose API credentials.")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        OpenAICompatibleProviderSection {
            id: mistralSection
            configPage: providersPage
            providerTitle: i18n("Mistral AI")
            enabledProp: "cfg_mistralEnabled"
            secretKey: "mistral"
            modelProp: "cfg_mistralModel"
            baseUrlProp: "cfg_mistralCustomBaseUrl"
            description: i18n("Read-only model discovery; inference tests are manual and may consume quota")
            keyPlaceholder: i18n("Enter Mistral API key...")
            modelOptions: providersPage.catalogModelIds("mistral")
        }

        OpenAICompatibleProviderSection {
            id: deepseekSection
            configPage: providersPage
            providerTitle: i18n("DeepSeek")
            enabledProp: "cfg_deepseekEnabled"
            secretKey: "deepseek"
            modelProp: "cfg_deepseekModel"
            baseUrlProp: "cfg_deepseekCustomBaseUrl"
            description: i18n("Tracks rate limits, token usage, and account balance")
            keyPlaceholder: i18n("Enter DeepSeek API key...")
            modelOptions: providersPage.catalogModelIds("deepseek")
        }

        OpenAICompatibleProviderSection {
            id: groqSection
            configPage: providersPage
            providerTitle: i18n("Groq")
            enabledProp: "cfg_groqEnabled"
            secretKey: "groq"
            modelProp: "cfg_groqModel"
            baseUrlProp: "cfg_groqCustomBaseUrl"
            description: i18n("OpenAI-compatible API with rate limit headers")
            keyPlaceholder: i18n("Enter Groq API key...")
            modelOptions: providersPage.catalogModelIds("groq")
        }

        OpenAICompatibleProviderSection {
            id: xaiSection
            configPage: providersPage
            providerTitle: i18n("xAI / Grok")
            enabledProp: "cfg_xaiEnabled"
            secretKey: "xai"
            modelProp: "cfg_xaiModel"
            baseUrlProp: "cfg_xaiCustomBaseUrl"
            description: i18n("OpenAI-compatible API for Grok models")
            keyPlaceholder: i18n("Enter xAI API key...")
            modelOptions: providersPage.catalogModelIds("xai")
        }

        OpenAICompatibleProviderSection {
            id: ollamaSection
            configPage: providersPage
            providerTitle: i18n("Ollama Cloud")
            enabledProp: "cfg_ollamaEnabled"
            secretKey: "ollama"
            modelProp: "cfg_ollamaModel"
            baseUrlProp: "cfg_ollamaCustomBaseUrl"
            description: i18n("Uses Ollama Cloud's OpenAI-compatible API at ollama.com/v1. Create an API key in your Ollama settings to monitor cloud usage from the widget.")
            keyPlaceholder: i18n("Create a key in ollama.com/settings")
            baseUrlTooltip: i18n("Override the Ollama Cloud API endpoint for proxies or gateways. Must start with https://")
            modelOptions: providersPage.catalogModelIds("ollama")
        }

        OpenAICompatibleProviderSection {
            id: openrouterSection
            configPage: providersPage
            providerTitle: i18n("OpenRouter")
            enabledProp: "cfg_openrouterEnabled"
            secretKey: "openrouter"
            modelProp: "cfg_openrouterModel"
            baseUrlProp: "cfg_openrouterCustomBaseUrl"
            description: i18n("Unified gateway to 600+ models. Shows credits balance and usage.")
            keyPlaceholder: i18n("sk-or-...")
            modelOptions: providersPage.catalogModelIds("openrouter")
        }

        OpenAICompatibleProviderSection {
            id: togetherSection
            configPage: providersPage
            providerTitle: i18n("Together AI")
            enabledProp: "cfg_togetherEnabled"
            secretKey: "together"
            modelProp: "cfg_togetherModel"
            baseUrlProp: "cfg_togetherCustomBaseUrl"
            description: i18n("Fast inference for open-source models (Llama, Qwen, DeepSeek)")
            keyPlaceholder: i18n("Enter Together AI API key...")
            modelOptions: providersPage.catalogModelIds("together")
        }

        OpenAICompatibleProviderSection {
            id: cohereSection
            configPage: providersPage
            providerTitle: i18n("Cohere")
            enabledProp: "cfg_cohereEnabled"
            secretKey: "cohere"
            modelProp: "cfg_cohereModel"
            baseUrlProp: "cfg_cohereCustomBaseUrl"
            description: i18n("Enterprise RAG and multilingual models via OpenAI-compatible API")
            keyPlaceholder: i18n("Enter Cohere API key...")
            modelOptions: providersPage.catalogModelIds("cohere")
        }

        OpenAICompatibleProviderSection {
            id: litellmSection
            configPage: providersPage
            providerTitle: i18n("LiteLLM Proxy")
            enabledProp: "cfg_litellmEnabled"; modelProp: "cfg_litellmModel"; baseUrlProp: "cfg_litellmCustomBaseUrl"
            secretKey: "litellm"
            description: i18n("Read-only gateway spend and token aggregation. Custom HTTPS endpoint required; loopback HTTP may be enabled explicitly.")
            keyPlaceholder: i18n("LiteLLM master or viewer key")
            modelOptions: providersPage.catalogModelIds("litellm")
        }
        OpenAICompatibleProviderSection {
            id: cerebrasSection
            configPage: providersPage
            providerTitle: i18n("Cerebras Inference")
            enabledProp: "cfg_cerebrasEnabled"; modelProp: "cfg_cerebrasModel"; baseUrlProp: "cfg_cerebrasCustomBaseUrl"
            secretKey: "cerebras"
            description: i18n("Read-only model discovery; dedicated metrics remain capability-dependent.")
            keyPlaceholder: i18n("Enter Cerebras API key")
            modelOptions: providersPage.catalogModelIds("cerebras")
        }
        OpenAICompatibleProviderSection {
            id: fireworksSection
            configPage: providersPage
            providerTitle: i18n("Fireworks AI")
            enabledProp: "cfg_fireworksEnabled"; modelProp: "cfg_fireworksModel"; baseUrlProp: "cfg_fireworksCustomBaseUrl"
            secretKey: "fireworks"
            description: i18n("Read-only model discovery. Scheduled billing is disabled until permissions are validated.")
            keyPlaceholder: i18n("Enter Fireworks API key; set account endpoint below")
            modelOptions: providersPage.catalogModelIds("fireworks")
        }
        OpenAICompatibleProviderSection {
            id: perplexitySection
            configPage: providersPage
            providerTitle: i18n("Perplexity API")
            enabledProp: "cfg_perplexityEnabled"; modelProp: "cfg_perplexityModel"; baseUrlProp: "cfg_perplexityCustomBaseUrl"
            description: i18n("Read-only Agent API model discovery; no automatic inference request.")
            keyPlaceholder: i18n("Public model discovery requires no key")
            requiresApiKey: false
            modelOptions: providersPage.catalogModelIds("perplexity")
        }

        // ── Google Veo ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Google Veo")
        }

        QQC2.Switch {
            id: googleveoSwitch
            Kirigami.FormData.label: i18n("Enable:")
            checked: plasmoid.configuration.googleveoEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("API Key:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: googleveoKeyField
                enabled: googleveoSwitch.checked && secrets.walletOpen
                echoMode: googleveoKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AIza...")
                Layout.fillWidth: true
                onTextEdited: providersPage.stageSecret("googleveo", text)
            }

            QQC2.ToolButton {
                id: googleveoKeyVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? i18n("Hide key") : i18n("Show key")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: secrets.walletOpen && googleveoKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: providersPage.clearSecret("googleveo", googleveoKeyField)
            }
        }

        QQC2.Label {
            visible: googleveoSwitch.checked
            text: i18n("Monitors Google Veo video generation API connectivity and tier limits")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.ComboBox {
            id: googleveoModelField
            Kirigami.FormData.label: i18n("Model:")
            enabled: googleveoSwitch.checked
            editable: true
            editText: plasmoid.configuration.googleveoModel
            model: providersPage.catalogModelIds("googleveo")
            onEditTextChanged: plasmoid.configuration.googleveoModel = editText
            property alias text: googleveoModelField.editText
        }

        QQC2.ComboBox {
            id: googleveoTierField
            Kirigami.FormData.label: i18n("Pricing tier:")
            enabled: googleveoSwitch.checked
            visible: providersPage.advancedMode
            model: [
                { text: i18n("Free"), value: "free" },
                { text: i18n("Paid (Pay-as-you-go)"), value: "paid" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: cfg_googleveoTier === "paid" ? 1 : 0
            onActivated: cfg_googleveoTier = currentValue
        }

        QQC2.Label {
            visible: googleveoSwitch.checked && googleveoTierField.currentIndex === 0
            text: i18n("Free tier has very limited video generation quotas.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        QQC2.TextField {
            id: googleveoBaseUrlField
            Kirigami.FormData.label: i18n("Custom base URL:")
            enabled: googleveoSwitch.checked
            visible: providersPage.advancedMode
            text: plasmoid.configuration.googleveoCustomBaseUrl
            placeholderText: i18n("Leave empty for default")
            Layout.fillWidth: true
            QQC2.ToolTip.text: i18n("Override the API endpoint for proxies or self-hosted gateways. Must start with https://")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(googleveoBaseUrlField.text)
            text: i18n("⚠ Use HTTPS, or HTTP only for a local loopback address")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: providersPage.isInvalidUrl(googleveoBaseUrlField.text)
                     && googleveoBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ External HTTP is blocked because it would expose API credentials.")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
