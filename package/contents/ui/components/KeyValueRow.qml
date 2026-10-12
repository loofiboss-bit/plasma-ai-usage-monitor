pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

Item {
    id: rowItem

    property string label: ""
    property string context: ""
    property string value: ""
    property string valueSubtitle: ""
    property color valueColor: Kirigami.Theme.textColor
    property string exactValue: ""

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight
    Accessible.role: Accessible.StaticText
    Accessible.name: (label ? label + ": " : "") + value + (context ? " (" + context + ")" : "")
    Accessible.description: exactValue

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: rowItem.label
                elide: Text.ElideRight
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: rowItem.context !== ""
                text: rowItem.context
                color: Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            Layout.minimumWidth: 0
            spacing: 0

            PlasmaComponents.Label {
                text: rowItem.value
                font.bold: true
                color: rowItem.valueColor
                horizontalAlignment: Text.AlignRight
                Layout.alignment: Qt.AlignRight
            }

            PlasmaComponents.Label {
                visible: rowItem.valueSubtitle !== ""
                text: rowItem.valueSubtitle
                color: Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                horizontalAlignment: Text.AlignRight
                Layout.alignment: Qt.AlignRight
            }
        }
    }
}
