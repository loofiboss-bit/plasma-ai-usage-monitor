import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "SourceVerification.js" as Verification

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
    readonly property string severity: Verification.severity(stateKey)
    type: severity === "positive" ? Kirigami.MessageType.Positive
        : severity === "warning" ? Kirigami.MessageType.Warning
        : severity === "error" ? Kirigami.MessageType.Error
        : Kirigami.MessageType.Information
    text: {
        var knownState = ["disabled", "unavailable_locally", "needs_configuration",
            "ready_to_verify", "verifying", "waiting_for_activity",
            "connected_connectivity_only", "reporting_estimate", "reporting_actual",
            "degraded", "failed"].indexOf(stateKey) >= 0;
        var detail = knownState && message.length > 0 ? message
            : Verification.message({readinessStateKey: stateKey});
        if (timestamp.length === 0) return detail;
        return detail + "\n" + i18n("Last verified: %1", timestamp);
    }
}
