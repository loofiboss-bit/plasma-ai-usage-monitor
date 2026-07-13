import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    id: section

    required property var configPage
    required property string providerTitle
    required property string enabledProp
    required property string modelProp
    required property string baseUrlProp
    required property string description
    required property string keyPlaceholder
    required property var modelOptions
    property string keyLabel: i18n("API Key:")
    property string modelLabel: i18n("Model:")
    property string baseUrlLabel: i18n("Custom base URL:")
    property string baseUrlTooltip: i18n("Override the API endpoint for proxies or self-hosted gateways. Must start with https://")
    property string baseUrlPlaceholder: i18n("Leave empty for default")
    property bool requiresApiKey: true
    property bool keyDirty: false
    property alias keyField: providerKeyField

    Layout.fillWidth: true

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: section.providerTitle
    }

    QQC2.Switch {
        id: enabledSwitch
        Kirigami.FormData.label: i18n("Enable:")
        checked: section.configPage[section.enabledProp]
        onToggled: section.configPage[section.enabledProp] = checked
    }

    RowLayout {
        visible: section.requiresApiKey
        Kirigami.FormData.label: section.keyLabel
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.TextField {
            id: providerKeyField
            enabled: enabledSwitch.checked
            echoMode: keyVisibleButton.checked ? TextInput.Normal : TextInput.Password
            placeholderText: section.keyPlaceholder
            Layout.fillWidth: true
            onTextEdited: section.keyDirty = true
        }

        QQC2.ToolButton {
            id: keyVisibleButton
            checkable: true
            checked: false
            icon.name: checked ? "password-show-off" : "password-show-on"
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: checked ? i18n("Hide key") : i18n("Show key")
            QQC2.ToolTip.visible: hovered
        }

        QQC2.ToolButton {
            icon.name: "edit-clear"
            enabled: providerKeyField.text.length > 0
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: i18n("Clear key")
            QQC2.ToolTip.visible: hovered
            onClicked: {
                providerKeyField.text = "";
                section.keyDirty = true;
            }
        }
    }

    QQC2.Label {
        visible: enabledSwitch.checked
        text: section.description
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        opacity: 0.6
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.ComboBox {
        id: modelField
        Kirigami.FormData.label: section.modelLabel
        enabled: enabledSwitch.checked
        editable: true
        editText: section.configPage[section.modelProp]
        model: section.modelOptions
        Layout.fillWidth: true
        onEditTextChanged: section.configPage[section.modelProp] = editText
        property alias text: modelField.editText
    }

    QQC2.TextField {
        id: baseUrlField
        Kirigami.FormData.label: section.baseUrlLabel
        enabled: enabledSwitch.checked
        visible: section.configPage.advancedMode
        text: section.configPage[section.baseUrlProp]
        placeholderText: section.baseUrlPlaceholder
        Layout.fillWidth: true
        QQC2.ToolTip.text: section.baseUrlTooltip
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: 500
        onTextChanged: section.configPage[section.baseUrlProp] = text
    }

    QQC2.Label {
        visible: section.configPage.advancedMode && section.configPage.isInvalidUrl(baseUrlField.text)
        text: i18n("URL must use HTTPS; HTTP is allowed only for a loopback endpoint")
        color: Kirigami.Theme.negativeTextColor
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.Label {
        visible: section.configPage.advancedMode
                 && (baseUrlField.text.toLowerCase().startsWith("http://localhost")
                     || baseUrlField.text.toLowerCase().startsWith("http://127.0.0.1")
                     || baseUrlField.text.toLowerCase().startsWith("http://[::1]"))
        text: i18n("Loopback HTTP is enabled for this explicitly local endpoint.")
        color: Kirigami.Theme.neutralTextColor
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
