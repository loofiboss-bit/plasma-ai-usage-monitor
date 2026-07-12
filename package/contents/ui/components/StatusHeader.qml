import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: header
    required property var providers
    required property var tools
    readonly property int enabledProviders: providers.filter(function(item) { return item.enabled; }).length
    readonly property int connectedProviders: providers.filter(function(item) {
        return item.enabled && item.backend?.connected;
    }).length
    readonly property int attentionCount: providers.filter(function(item) {
        return item.enabled && (!!item.backend?.error || item.backend?.providerState === 5);
    }).length + tools.filter(function(item) {
        return item.enabled && (item.monitor?.limitReached || !(item.monitor?.installed ?? true));
    }).length

    implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.06)
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.2)

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        PlasmaExtras.Heading {
            level: 4
            Layout.fillWidth: true
            text: i18n("%1 of %2 providers connected", header.connectedProviders, header.enabledProviders)
        }
        PlasmaComponents.Label {
            text: header.attentionCount > 0 ? i18np("%1 item needs attention", "%1 items need attention", header.attentionCount) : i18n("All configured sources are ready")
            color: header.attentionCount > 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
        }
    }
}
