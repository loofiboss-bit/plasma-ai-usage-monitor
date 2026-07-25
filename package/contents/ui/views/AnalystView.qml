import QtQuick
import ".." as Monitor

Item {
    property var monitor: null
    Monitor.AnalystTab {
        anchors.fill: parent
        monitor: parent.monitor
    }
}
