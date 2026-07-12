import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: list
    required property var providers
    required property var tools
    spacing: Kirigami.Units.smallSpacing

    Repeater {
        model: list.providers
        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: modelData.enabled && (!!modelData.backend?.error || modelData.backend?.providerState === 5)
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.negativeTextColor
            text: modelData.backend?.error
                ? i18n("%1: %2", modelData.name, modelData.backend.error)
                : i18n("%1 data is stale", modelData.name)
        }
    }
    Repeater {
        model: list.tools
        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: modelData.enabled && (modelData.monitor?.limitReached || !(modelData.monitor?.installed ?? true))
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.neutralTextColor
            text: modelData.monitor?.limitReached
                ? i18n("%1 reached its configured limit", modelData.name)
                : i18n("%1 is not installed", modelData.name)
        }
    }
}
