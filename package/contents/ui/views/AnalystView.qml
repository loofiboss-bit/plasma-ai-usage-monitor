import QtQuick
import ".." as Monitor

Item {
    id: analystView
    property var monitor: null
    property var sessionState: null
    property var viewState: null

    function captureViewState() {
        return analystPage.captureViewState();
    }

    Monitor.AnalystTab {
        id: analystPage
        anchors.fill: parent
        monitor: analystView.monitor
        sessionState: analystView.sessionState
        viewState: analystView.viewState
    }
}
