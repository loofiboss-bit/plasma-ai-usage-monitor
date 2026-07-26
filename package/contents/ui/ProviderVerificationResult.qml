import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.InlineMessage {
    id: result

    required property string sourceId
    required property string resultSourceId
    required property string stateKey
    required property string message
    property string timestamp: ""

    readonly property bool appliesToSource: sourceId.length > 0 && sourceId === resultSourceId
    visible: appliesToSource
    Layout.fillWidth: true
    type: stateKey === "failed" || stateKey === "degraded"
        || stateKey === "needs_configuration" ? Kirigami.MessageType.Error
        : stateKey === "verifying" ? Kirigami.MessageType.Information
        : Kirigami.MessageType.Positive
    text: {
        var detail = message.length > 0 ? message : i18n("Verification result: %1", stateKey);
        if (timestamp.length === 0) return detail;
        return detail + "\n" + i18n("Last verified: %1", timestamp);
    }
}
