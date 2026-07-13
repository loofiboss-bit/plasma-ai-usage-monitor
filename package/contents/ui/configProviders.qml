import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: providersPage

    property bool advancedMode: plasmoid.configuration.advancedSettingsMode

    onAdvancedModeChanged: {
        plasmoid.configuration.advancedSettingsMode = advancedMode
    }

    // URL validation helper
    function isInvalidUrl(url) {
        if (url.length === 0) return false;
        var lower = url.toLowerCase();
        return !lower.startsWith("https://") && !lower.startsWith("http://");
    }

    // Keep every visible model picker aligned with Provider Catalog v4.
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

    // Track whether the user has actually edited each key field
    property bool openaiKeyDirty: false
    property bool anthropicKeyDirty: false
    property bool googleKeyDirty: false
    property bool mistralKeyDirty: false
    property bool deepseekKeyDirty: false
    property bool groqKeyDirty: false
    property bool xaiKeyDirty: false
    property bool ollamaKeyDirty: false
    property bool openrouterKeyDirty: false
    property bool togetherKeyDirty: false
    property bool cohereKeyDirty: false
    property bool googleveoKeyDirty: false
    property bool azureKeyDirty: false
    property bool bedrockAccessKeyDirty: false
    property bool bedrockSecretKeyDirty: false
    property bool bedrockSessionTokenDirty: false

    // ── KWallet Integration ──
    SecretsManager {
        id: secrets

        onWalletOpenChanged: {
            if (walletOpen) {
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

    function loadKeys() {
        var providers = [
            { name: "openai", field: openaiKeyField, dirtyProp: "openaiKeyDirty" },
            { name: "anthropic", field: anthropicKeyField, dirtyProp: "anthropicKeyDirty" },
            { name: "google", field: googleKeyField, dirtyProp: "googleKeyDirty" },
            { name: "mistral", field: mistralSection.keyField, dirtyProp: "mistralKeyDirty" },
            { name: "deepseek", field: deepseekSection.keyField, dirtyProp: "deepseekKeyDirty" },
            { name: "groq", field: groqSection.keyField, dirtyProp: "groqKeyDirty" },
            { name: "xai", field: xaiSection.keyField, dirtyProp: "xaiKeyDirty" },
            { name: "ollama", field: ollamaSection.keyField, dirtyProp: "ollamaKeyDirty" },
            { name: "openrouter", field: openrouterSection.keyField, dirtyProp: "openrouterKeyDirty" },
            { name: "together", field: togetherSection.keyField, dirtyProp: "togetherKeyDirty" },
            { name: "cohere", field: cohereSection.keyField, dirtyProp: "cohereKeyDirty" },
            { name: "googleveo", field: googleveoKeyField, dirtyProp: "googleveoKeyDirty" },
            { name: "azure", field: azureKeyField, dirtyProp: "azureKeyDirty" },
            { name: "bedrock_access_key_id", field: bedrockAccessKeyField, dirtyProp: "bedrockAccessKeyDirty" },
            { name: "bedrock_secret_access_key", field: bedrockSecretKeyField, dirtyProp: "bedrockSecretKeyDirty" },
            { name: "bedrock_session_token", field: bedrockSessionTokenField, dirtyProp: "bedrockSessionTokenDirty" }
        ];

        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (secrets.hasKey(p.name)) {
                p.field.text = "********";
                providersPage[p.dirtyProp] = false;
            } else {
                p.field.text = "";
            }
        }
    }

    function saveKeys() {
        var providers = [
            { name: "openai", field: openaiKeyField, dirty: openaiKeyDirty },
            { name: "anthropic", field: anthropicKeyField, dirty: anthropicKeyDirty },
            { name: "google", field: googleKeyField, dirty: googleKeyDirty },
            { name: "mistral", field: mistralSection.keyField, dirty: mistralSection.keyDirty },
            { name: "deepseek", field: deepseekSection.keyField, dirty: deepseekSection.keyDirty },
            { name: "groq", field: groqSection.keyField, dirty: groqSection.keyDirty },
            { name: "xai", field: xaiSection.keyField, dirty: xaiSection.keyDirty },
            { name: "ollama", field: ollamaSection.keyField, dirty: ollamaSection.keyDirty },
            { name: "openrouter", field: openrouterSection.keyField, dirty: openrouterSection.keyDirty },
            { name: "together", field: togetherSection.keyField, dirty: togetherSection.keyDirty },
            { name: "cohere", field: cohereSection.keyField, dirty: cohereSection.keyDirty },
            { name: "googleveo", field: googleveoKeyField, dirty: googleveoKeyDirty },
            { name: "azure", field: azureKeyField, dirty: azureKeyDirty },
            { name: "bedrock_access_key_id", field: bedrockAccessKeyField, dirty: bedrockAccessKeyDirty },
            { name: "bedrock_secret_access_key", field: bedrockSecretKeyField, dirty: bedrockSecretKeyDirty },
            { name: "bedrock_session_token", field: bedrockSessionTokenField, dirty: bedrockSessionTokenDirty }
        ];

        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (p.dirty && p.field.text.length > 0 && p.field.text !== "********") {
                secrets.storeKey(p.name, p.field.text);
            } else if (p.dirty && p.field.text.length === 0) {
                secrets.removeKey(p.name);
            }
        }
    }

    Component.onCompleted: {
        if (secrets.walletOpen) {
            loadKeys();
        }
    }

    Component.onDestruction: {
        saveKeys();
    }

    
    Kirigami.FormLayout {
        anchors.fill: parent
        
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
                enabled: openaiSwitch.checked
                echoMode: openaiKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("sk-admin-...")
                Layout.fillWidth: true
                onTextEdited: providersPage.openaiKeyDirty = true
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
                enabled: openaiKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: { openaiKeyField.text = ""; providersPage.openaiKeyDirty = true; }
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
            text: i18n("⚠ URL must start with https:// or http://")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: openaiBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ Using HTTP is insecure. API keys will be sent unencrypted.")
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
                enabled: azureSwitch.checked
                echoMode: azureKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("Enter Azure OpenAI API key...")
                Layout.fillWidth: true
                onTextEdited: providersPage.azureKeyDirty = true
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
                enabled: azureKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: { azureKeyField.text = ""; providersPage.azureKeyDirty = true; }
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
            text: i18n("⚠ URL must start with https:// or http://")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: azureBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ Using HTTP is insecure. API keys will be sent unencrypted.")
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
                enabled: bedrockSwitch.checked
                echoMode: bedrockAccessKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AKIA...")
                Layout.fillWidth: true
                onTextEdited: providersPage.bedrockAccessKeyDirty = true
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
                enabled: bedrockSwitch.checked
                echoMode: bedrockSecretKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AWS secret access key")
                Layout.fillWidth: true
                onTextEdited: providersPage.bedrockSecretKeyDirty = true
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
            enabled: bedrockSwitch.checked
            echoMode: TextInput.Password
            placeholderText: i18n("Optional for temporary credentials")
            Layout.fillWidth: true
            onTextEdited: providersPage.bedrockSessionTokenDirty = true
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
                enabled: anthropicSwitch.checked
                echoMode: anthropicKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("sk-ant-...")
                Layout.fillWidth: true
                onTextEdited: providersPage.anthropicKeyDirty = true
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
                enabled: anthropicKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: { anthropicKeyField.text = ""; providersPage.anthropicKeyDirty = true; }
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
            text: i18n("⚠ URL must start with https:// or http://")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: anthropicBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ Using HTTP is insecure. API keys will be sent unencrypted.")
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
                enabled: googleSwitch.checked
                echoMode: googleKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AIza...")
                Layout.fillWidth: true
                onTextEdited: providersPage.googleKeyDirty = true
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
                enabled: googleKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: { googleKeyField.text = ""; providersPage.googleKeyDirty = true; }
            }
        }

        QQC2.Label {
            visible: googleSwitch.checked
            text: i18n("Shows connectivity status and known tier limits")
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
            text: i18n("⚠ URL must start with https:// or http://")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: googleBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ Using HTTP is insecure. API keys will be sent unencrypted.")
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
            modelProp: "cfg_mistralModel"
            baseUrlProp: "cfg_mistralCustomBaseUrl"
            description: i18n("Rate limits and token usage via chat/completions endpoint")
            keyPlaceholder: i18n("Enter Mistral API key...")
            modelOptions: providersPage.catalogModelIds("mistral")
        }

        OpenAICompatibleProviderSection {
            id: deepseekSection
            configPage: providersPage
            providerTitle: i18n("DeepSeek")
            enabledProp: "cfg_deepseekEnabled"
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
            modelProp: "cfg_cohereModel"
            baseUrlProp: "cfg_cohereCustomBaseUrl"
            description: i18n("Enterprise RAG and multilingual models via OpenAI-compatible API")
            keyPlaceholder: i18n("Enter Cohere API key...")
            modelOptions: providersPage.catalogModelIds("cohere")
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
                enabled: googleveoSwitch.checked
                echoMode: googleveoKeyVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("AIza...")
                Layout.fillWidth: true
                onTextEdited: providersPage.googleveoKeyDirty = true
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
                enabled: googleveoKeyField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear key"); QQC2.ToolTip.visible: hovered
                onClicked: { googleveoKeyField.text = ""; providersPage.googleveoKeyDirty = true; }
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
            text: i18n("⚠ URL must start with https:// or http://")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: googleveoBaseUrlField.text.toLowerCase().startsWith("http://")
            text: i18n("⚠ Using HTTP is insecure. API keys will be sent unencrypted.")
            color: Kirigami.Theme.negativeTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
