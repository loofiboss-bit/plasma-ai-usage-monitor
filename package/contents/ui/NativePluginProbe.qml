import QtQuick
import com.github.loofi.aiusagemonitor 1.0 as AIUsageMonitor

Item {
    readonly property string pluginVersion: AIUsageMonitor.AppInfo.version
    visible: false
    width: 0
    height: 0
}
