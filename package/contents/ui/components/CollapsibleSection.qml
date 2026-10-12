pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: sectionRoot

    property string title: ""
    property string summaryText: ""
    property bool expanded: false
    default property alias content: contentContainer.data

    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing

    QQC2.AbstractButton {
        id: headerButton
        Layout.fillWidth: true
        activeFocusOnTab: true
        checkable: true
        checked: sectionRoot.expanded
        onClicked: sectionRoot.expanded = !sectionRoot.expanded

        Accessible.role: Accessible.Button
        Accessible.name: sectionRoot.title + (sectionRoot.summaryText ? " · " + sectionRoot.summaryText : "")
        Accessible.description: sectionRoot.expanded ? i18n("Collapse section") : i18n("Expand section")

        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: sectionRoot.expanded ? "arrow-down" : "arrow-right"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: width
            }

            PlasmaExtras.Heading {
                level: 4
                text: sectionRoot.title
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: sectionRoot.summaryText !== ""
                text: sectionRoot.summaryText
                color: Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
            }
        }
    }

    ColumnLayout {
        id: contentContainer
        Layout.fillWidth: true
        visible: sectionRoot.expanded
        spacing: Kirigami.Units.smallSpacing
    }
}
