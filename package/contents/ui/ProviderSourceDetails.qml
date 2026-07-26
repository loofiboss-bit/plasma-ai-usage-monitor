pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    id: details

    required property var descriptor
    required property var controller
    required property var configPage

    readonly property bool hasSource: !!descriptor.configKey
    readonly property bool sourceEnabled: hasSource
        && !!controller.value(descriptor.enabledConfigKey)
    readonly property var credentialSlots: hasSource
        ? (descriptor.auth.credentialSlots || []) : []
    readonly property string baseUrlKey: hasSource
        ? descriptor.customBaseUrlConfigKey : ""
    readonly property bool localTool: descriptor.sourceKind === "local_tool"

    Layout.fillWidth: true

    function credentialLabel(slot) {
        var labels = {
            "openai": i18n("Admin API key"),
            "anthropic": i18n("Standard API key (connectivity only)"),
            "anthropic_admin": i18n("Admin API key (usage and cost)"),
            "azure_openai_api_key": i18n("API key"),
            "bedrock_access_key_id": i18n("Access key ID"),
            "bedrock_secret_access_key": i18n("Secret access key"),
            "bedrock_session_token": i18n("Session token (optional)")
        };
        return labels[slot] || i18n("API key");
    }

    function permissionLabel(auth) {
        if (details.localTool)
            return i18n("Read local activity metadata; no provider credential or inference request");
        if (descriptor.configKey === "anthropic")
            return i18n("A standard key checks connectivity. An optional organization Admin key reads actual usage and cost reports.");
        var scheme = auth.scheme || "none";
        if (scheme === "none") return i18n("No credential required");
        if (descriptor.monitoringLevel === "actual_usage_spend")
            return i18n("Read-only organization usage and costs permission");
        if (descriptor.monitoringLevel === "gateway_aggregate")
            return i18n("Read-only gateway spend-log permission");
        if (descriptor.monitoringLevel === "balance_connectivity")
            return i18n("Read-only balance and model discovery permission");
        return i18n("Read-only model discovery permission");
    }

    function endpointSummary(safeRefresh) {
        if (details.localTool) return i18n("Local installation and activity check");
        var paths = safeRefresh.paths || [safeRefresh.path || ""];
        var calls = [];
        for (var i = 0; i < paths.length; ++i) {
            if (paths[i]) calls.push((safeRefresh.method || "GET") + " " + paths[i]);
        }
        return calls.length > 0 ? calls.join(" · ") : i18n("No scheduled request");
    }

    Kirigami.Heading {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: ""
        text: details.hasSource ? details.descriptor.name : i18n("Select a source")
        level: 2
        Accessible.name: text
    }

    QQC2.Label {
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Monitoring level:")
        text: details.controller.categoryLabel(details.descriptor.monitoringLevel)
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.Label {
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Required permission:")
        text: details.permissionLabel(details.descriptor.auth || ({}))
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.Label {
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Scheduled check:")
        text: details.endpointSummary(details.descriptor.safeRefresh || ({}))
        wrapMode: Text.WrapAnywhere
        font.family: "monospace"
        Layout.fillWidth: true
    }

    QQC2.Switch {
        visible: details.hasSource
        Kirigami.FormData.label: i18n("Enabled:")
        checked: details.sourceEnabled
        text: checked ? i18n("Scheduled monitoring is enabled")
                      : i18n("Scheduled monitoring is disabled")
        Accessible.name: i18n("Enable %1", details.descriptor.name || i18n("source"))
        onToggled: details.controller.setValue(details.descriptor.enabledConfigKey, checked)
    }

    Repeater {
        model: details.credentialSlots

        CredentialEditor {
            required property string modelData
            Kirigami.FormData.label: details.credentialLabel(modelData) + ":"
            label: details.credentialLabel(modelData)
            editable: details.sourceEnabled && details.configPage.walletOpen
            clearEnabled: details.configPage.hasStoredOrPendingSecret(modelData)
            placeholderText: modelData === "bedrock_session_token"
                && !details.configPage.hasStoredOrPendingSecret(modelData)
                ? i18n("Optional")
                : details.descriptor.configKey === "anthropic"
                    && !details.configPage.hasStoredOrPendingSecret(modelData)
                    ? i18n("Configure either key")
                    : details.configPage.secretPlaceholder(modelData)
            onCredentialEdited: function(value) {
                details.configPage.stageSecret(modelData, value);
            }
            onClearRequested: details.configPage.removeSecret(modelData)
        }
    }

    QQC2.Label {
        visible: details.credentialSlots.length > 0 && !details.configPage.walletOpen
        text: i18n("Unlock KDE Wallet to edit credentials.")
        color: Kirigami.Theme.negativeTextColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.ComboBox {
        visible: details.hasSource && !details.localTool && details.configPage.advancedMode
        Kirigami.FormData.label: i18n("Model override:")
        enabled: details.sourceEnabled
        editable: true
        model: details.hasSource ? details.configPage.catalogModelIds(details.descriptor.configKey) : []
        editText: details.hasSource
            ? String(details.controller.value(details.descriptor.modelConfigKey) || "") : ""
        Accessible.name: i18n("Model override for %1", details.descriptor.name || i18n("source"))
        Layout.fillWidth: true
        onEditTextChanged: {
            if (details.hasSource)
                details.controller.setValue(details.descriptor.modelConfigKey, editText);
        }
    }

    QQC2.TextField {
        visible: details.hasSource && !details.localTool && details.configPage.advancedMode
        Kirigami.FormData.label: i18n("Custom base URL:")
        enabled: details.sourceEnabled
        text: details.baseUrlKey ? String(details.controller.value(details.baseUrlKey) || "") : ""
        placeholderText: i18n("Leave empty for the catalog endpoint")
        inputMethodHints: Qt.ImhUrlCharactersOnly
        Accessible.name: i18n("Custom base URL for %1", details.descriptor.name || i18n("source"))
        Layout.fillWidth: true
        onTextEdited: details.controller.setValue(details.baseUrlKey, text)
    }

    QQC2.Label {
        visible: details.hasSource && details.configPage.advancedMode
                 && details.configPage.isInvalidUrl(String(details.controller.value(details.baseUrlKey) || ""))
        text: i18n("Use HTTPS. HTTP is allowed only for a loopback endpoint.")
        color: Kirigami.Theme.negativeTextColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.TextField {
        visible: details.configPage.advancedMode && details.descriptor.configKey === "openai"
        Kirigami.FormData.label: i18n("Project ID:")
        enabled: details.sourceEnabled
        text: String(details.controller.value("openaiProjectId") || "")
        placeholderText: "proj_..."
        Accessible.name: i18n("OpenAI project ID")
        Layout.fillWidth: true
        onTextEdited: details.controller.setValue("openaiProjectId", text)
    }

    QQC2.TextField {
        visible: details.configPage.advancedMode && details.descriptor.configKey === "azure"
        Kirigami.FormData.label: i18n("Deployment ID:")
        enabled: details.sourceEnabled
        text: String(details.controller.value("azureDeploymentId") || "")
        Accessible.name: i18n("Azure OpenAI deployment ID")
        Layout.fillWidth: true
        onTextEdited: details.controller.setValue("azureDeploymentId", text)
    }

    QQC2.TextField {
        visible: details.configPage.advancedMode && details.descriptor.configKey === "bedrock"
        Kirigami.FormData.label: i18n("Region:")
        enabled: details.sourceEnabled
        text: String(details.controller.value("bedrockRegion") || "")
        placeholderText: "us-east-1"
        Accessible.name: i18n("AWS Bedrock region")
        Layout.fillWidth: true
        onTextEdited: details.controller.setValue("bedrockRegion", text)
    }

    QQC2.ComboBox {
        visible: details.configPage.advancedMode
                 && (details.descriptor.configKey === "google"
                     || details.descriptor.configKey === "googleveo")
        Kirigami.FormData.label: i18n("Pricing tier:")
        enabled: details.sourceEnabled
        textRole: "text"
        valueRole: "value"
        Accessible.name: i18n("Pricing tier for %1", details.descriptor.name || i18n("source"))
        model: [
            { text: i18n("Free tier"), value: "free" },
            { text: i18n("Paid (pay as you go)"), value: "paid" }
        ]
        currentIndex: {
            var key = details.descriptor.configKey === "google" ? "googleTier" : "googleveoTier";
            return details.controller.value(key) === "paid" ? 1 : 0;
        }
        onActivated: {
            var key = details.descriptor.configKey === "google" ? "googleTier" : "googleveoTier";
            details.controller.setValue(key, currentValue);
        }
    }

    ProviderVerificationResult {
        Kirigami.FormData.isSection: true
        sourceId: details.descriptor.configKey || ""
        resultSourceId: details.configPage.verificationSourceId
        stateKey: details.configPage.verificationState
        message: details.configPage.verificationMessage
        timestamp: details.configPage.verificationTimestamp
    }

    RowLayout {
        visible: details.hasSource
        Kirigami.FormData.isSection: true
        Layout.fillWidth: true

        QQC2.Label {
            Layout.fillWidth: true
            visible: details.configPage.hasUnsavedChanges
            text: i18n("Apply the pending changes before verification.")
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }

        QQC2.Button {
            text: i18n("Verify")
            icon.name: "security-high"
            enabled: details.sourceEnabled && !details.configPage.hasUnsavedChanges
            Accessible.name: i18n("Verify %1 using its read-only scheduled check", details.descriptor.name)
            onClicked: details.configPage.requestVerification(details.descriptor.configKey)
        }
    }
}
