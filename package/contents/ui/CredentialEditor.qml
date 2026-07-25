import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

RowLayout {
    id: editor

    required property string label
    property string placeholderText: ""
    property bool editable: true
    property bool clearEnabled: false
    property bool showClearAction: true

    signal credentialEdited(string value)
    signal clearRequested()

    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing

    QQC2.TextField {
        id: credentialField
        Layout.fillWidth: true
        enabled: editor.editable
        echoMode: revealButton.checked ? TextInput.Normal : TextInput.Password
        placeholderText: editor.placeholderText
        Accessible.name: editor.label
        onTextEdited: editor.credentialEdited(text)
    }

    QQC2.ToolButton {
        id: revealButton
        enabled: editor.editable
        checkable: true
        icon.name: checked ? "password-show-off" : "password-show-on"
        display: QQC2.AbstractButton.IconOnly
        Accessible.name: checked ? KI18n.i18n("Hide %1", editor.label) : KI18n.i18n("Show %1", editor.label)
        QQC2.ToolTip.text: Accessible.name
        QQC2.ToolTip.visible: hovered
    }

    QQC2.ToolButton {
        visible: editor.showClearAction
        enabled: editor.editable && editor.clearEnabled
        icon.name: "edit-clear"
        display: QQC2.AbstractButton.IconOnly
        Accessible.name: KI18n.i18n("Clear %1", editor.label)
        QQC2.ToolTip.text: Accessible.name
        QQC2.ToolTip.visible: hovered
        onClicked: {
            credentialField.clear();
            editor.clearRequested();
        }
    }
}
