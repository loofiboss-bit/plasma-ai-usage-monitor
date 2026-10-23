import QtQuick
import QtTest
import org.kde.kirigami as Kirigami
import "../../../../package/contents/ui" as AppUi
import "../../../../package/contents/ui/SourceVerification.js" as Verification

TestCase {
    name: "SourceVerification"
    when: windowShown
    visible: true

    Component {
        id: resultComponent
        AppUi.ProviderVerificationResult {
            sourceId: "codex-cli"
            resultSourceId: "codex-cli"
            stateKey: ""
            message: ""
            width: 420
        }
    }

    function test_explicitStatusPresentation_data() {
        return [
            {tag: "disabled", severity: Kirigami.MessageType.Information},
            {tag: "unavailable_locally", severity: Kirigami.MessageType.Warning},
            {tag: "needs_configuration", severity: Kirigami.MessageType.Warning},
            {tag: "ready_to_verify", severity: Kirigami.MessageType.Information},
            {tag: "verifying", severity: Kirigami.MessageType.Information},
            {tag: "waiting_for_activity", severity: Kirigami.MessageType.Information},
            {tag: "connected_connectivity_only", severity: Kirigami.MessageType.Positive},
            {tag: "reporting_estimate", severity: Kirigami.MessageType.Positive},
            {tag: "reporting_actual", severity: Kirigami.MessageType.Positive},
            {tag: "degraded", severity: Kirigami.MessageType.Warning},
            {tag: "failed", severity: Kirigami.MessageType.Error},
            {tag: "future_unknown_state", severity: Kirigami.MessageType.Information},
            {tag: "", severity: Kirigami.MessageType.Information}
        ];
    }

    function test_explicitStatusPresentation(data) {
        var result = createTemporaryObject(resultComponent, this, {stateKey: data.tag});
        verify(result !== null);
        compare(result.type, data.severity);
        verify(result.text.length > 0);
        if (data.tag.indexOf("_") >= 0) verify(result.text.indexOf(data.tag) < 0);
        verify(result.text.indexOf("Verification result:") < 0);
        compare(result.text, Verification.message({readinessStateKey: data.tag}));
    }

    function test_unknownStatusNeverUsesOldPositiveMessage() {
        var result = createTemporaryObject(resultComponent, this, {
            stateKey: "future_unknown_state", message: "Verification succeeded."
        });
        compare(result.type, Kirigami.MessageType.Information);
        verify(result.text.indexOf("succeeded") < 0);
        verify(result.text.indexOf("unavailable") >= 0);
    }

    function test_waitingForActivityIsNotFailure() {
        var text = Verification.message({readinessStateKey: "waiting_for_activity"});
        verify(text.indexOf("activity") >= 0);
        verify(text.indexOf("failed") < 0);
    }

    function test_actionMessageAndSourceIsolation() {
        var text = Verification.message({
            readinessStateKey: "needs_configuration", nextActionText: "Add the required credentials."
        });
        compare(text, "Add the required credentials.");
        var result = createTemporaryObject(resultComponent, this, {
            stateKey: "needs_configuration", message: text, timestamp: "2026-10-23T12:00:00Z"
        });
        verify(result.visible);
        verify(result.text.indexOf(text) === 0);
        verify(result.text.indexOf("2026-10-23") > 0);
        result.resultSourceId = "other-source";
        verify(!result.visible);
    }
}
