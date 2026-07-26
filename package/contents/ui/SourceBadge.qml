import QtQuick
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

Rectangle {
    id: badge

    property string text: i18n("Unknown")

    radius: 4
    implicitWidth: badgeLabel.implicitWidth + Kirigami.Units.smallSpacing * 2
    implicitHeight: badgeLabel.implicitHeight + Kirigami.Units.smallSpacing
    color: Qt.alpha(badgeColor(), 0.12)
    border.width: 1
    border.color: Qt.alpha(badgeColor(), 0.38)

    PlasmaComponents.Label {
        id: badgeLabel
        anchors.centerIn: parent
        text: badge.text || i18n("Unknown")
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        font.bold: true
        color: badge.badgeColor()
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        maximumLineCount: 1
    }

    function badgeColor() {
        if (text === "Actual billing" || text === "Actual usage" || text === "Synced" || text === "Official") return Kirigami.Theme.positiveTextColor;
        if (text === "Needs review" || text === "Source conflict" || text === "Unknown pricing") return Kirigami.Theme.neutralTextColor;
        if (text === "Custom" || text === "Self-tracked" || text === "Browser sync") return Kirigami.Theme.linkColor;
        if (text === "Estimated" || text === "Probe only" || text === "Unknown") return Kirigami.Theme.disabledTextColor;
        return Kirigami.Theme.textColor;
    }
}
