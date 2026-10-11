import QtQuick
import ".." as Monitor

Item {
    property var monitor: null
    property var sessionState: null
    Monitor.AnalystTab {
        anchors.fill: parent
        monitor: parent.monitor
        sessionState: parent.sessionState
    }
}
