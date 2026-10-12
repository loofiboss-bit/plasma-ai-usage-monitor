pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "../Utils.js" as Utils

Rectangle {
    id: root

    property string title: ""
    property var value: ""
    property string formattedValue: ""
    property string subtitle: ""
    property bool isCount: false
    property bool prominent: false
    property color valueColor: Kirigami.Theme.textColor

    Layout.fillWidth: true
    Layout.minimumWidth: Kirigami.Units.gridUnit * 5
    implicitHeight: tileLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
    radius: Kirigami.Units.smallSpacing
    color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.65)
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.1)

    Accessible.role: Accessible.StaticText
    Accessible.name: title + ": " + displayValue() + (subtitle ? " (" + subtitle + ")" : "")

    function displayValue() {
        if (formattedValue !== "") return formattedValue;
        if (value === undefined || value === null || value === "") return "\u2014";
        if (typeof value === "string") return value;
        return isCount ? Utils.formatCount(value) : Utils.formatNumber(value);
    }

    ColumnLayout {
        id: tileLayout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: 2

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: root.title
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: root.displayValue()
            font.bold: true
            color: root.valueColor
            font.pointSize: root.prominent
                ? Kirigami.Theme.defaultFont.pointSize * 1.3
                : Kirigami.Theme.defaultFont.pointSize
            elide: Text.ElideRight
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: root.subtitle !== ""
            text: root.subtitle
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
        }
    }
}
