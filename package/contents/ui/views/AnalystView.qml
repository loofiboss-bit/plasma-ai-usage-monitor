import QtQuick
import ".." as Monitor

Item {
    id: analystView
    property var monitor: null
    property var sessionState: null
    property var viewState: null
    signal sessionHistoryRequested(string toolId, string projectId, int rangeDays, string sessionId, int pageOffset, var modelFilter)

    function captureViewState() {
        return analystPage.captureViewState();
    }

    Monitor.AnalystTab {
        id: analystPage
        anchors.fill: parent
        monitor: analystView.monitor
        sessionState: analystView.sessionState
        viewState: analystView.viewState
        onSessionHistoryRequested: function(toolId, projectId, rangeDays, sessionId, pageOffset, modelFilter) {
            analystView.sessionHistoryRequested(toolId, projectId, rangeDays, sessionId, pageOffset, modelFilter);
        }
    }
}
