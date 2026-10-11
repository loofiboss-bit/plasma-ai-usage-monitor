import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: tests
    name: "LocalSessionPanelRefresh"
    visible: true
    when: windowShown
    width: 1200
    height: 900

    QtObject {
        id: fakeModel
        property bool codexEnabled: true
        property bool claudeCodeEnabled: false
        property int requests: 0
        signal importFinished(string toolId, int importedCount, bool partial, string errorKey)
        function requestProjects(requestId) { requests++; }
        function requestSummary(requestId, toolId, from, to, project) { requests++; }
        function requestSessions(requestId, toolId, from, to, project, offset, limit) { requests++; }
        function requestDaily(requestId, toolId, from, to, project, zone) { requests++; }
    }

    Component {
        id: panelComponent
        Components.LocalSessionPanel {
            sessionModel: fakeModel
            width: 1200
        }
    }

    function test_importRefreshesVisiblePanelAndRejectsOldListReply() {
        var panel = createTemporaryObject(panelComponent, tests);
        verify(panel !== null);
        var before = fakeModel.requests;
        var oldRequest = panel.sessionsRequestId;
        fakeModel.importFinished("codex-cli", 1, false, "");
        verify(fakeModel.requests > before);
        verify(panel.sessionsRequestId !== oldRequest);
        panel.acceptSessionRows(oldRequest, [{sessionId: "stale"}]);
        compare(panel.sessionRows.length, 0);
        panel.acceptSessionRows(panel.sessionsRequestId, [{sessionId: "current"}]);
        compare(panel.sessionRows[0].sessionId, "current");
        panel.visible = false;
        before = fakeModel.requests;
        fakeModel.importFinished("codex-cli", 1, false, "");
        compare(fakeModel.requests, before);
        panel.visible = true;
        verify(fakeModel.requests > before);
    }
}
