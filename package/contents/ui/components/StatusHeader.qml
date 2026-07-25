pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: header

    required property var presentation
    readonly property var facts: presentation.factRows()
    readonly property bool narrow: width < Kirigami.Units.gridUnit * 22

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.06)
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.2)
    Accessible.role: Accessible.StaticText
    Accessible.name: presentation.headline() + ". " + presentation.summaryText()

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            objectName: "dailyHeadline"
            level: 3
            Layout.fillWidth: true
            text: header.presentation.headline()
            wrapMode: Text.WordWrap
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: header.presentation.explanation()
            wrapMode: Text.WordWrap
            color: header.presentation.summary.attentionSourceCount > 0
                ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
        }

        GridLayout {
            Layout.fillWidth: true
            columns: header.narrow ? 1 : Math.max(1, header.facts.length)
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            visible: header.facts.length > 0

            Repeater {
                model: header.facts

                RowLayout {
                    id: factRow
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: factRow.modelData.icon
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: width
                    }
                    PlasmaComponents.Label {
                        text: factRow.modelData.value
                        font.bold: true
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: factRow.modelData.label
                        elide: Text.ElideRight
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
    }
}
