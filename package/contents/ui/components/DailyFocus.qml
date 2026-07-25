pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: focus

    required property var presentation
    readonly property var actionRow: presentation.topAction
    readonly property var facts: presentation.focusFacts()
    signal actionRequested(string stableId, string actionKey, string sourceKind)

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Qt.alpha(actionRow.attentionSeverity === "critical"
                    ? Kirigami.Theme.negativeBackgroundColor
                    : actionRow.attentionSeverity === "warning"
                      ? Kirigami.Theme.neutralBackgroundColor
                      : Kirigami.Theme.highlightColor, 0.12)
    border.width: 1
    border.color: Qt.alpha(actionRow.attentionSeverity === "critical"
                           ? Kirigami.Theme.negativeTextColor
                           : actionRow.attentionSeverity === "warning"
                             ? Kirigami.Theme.neutralTextColor
                             : Kirigami.Theme.highlightColor, 0.3)
    Accessible.role: Accessible.Grouping
    Accessible.name: presentation.headline() + ". "
        + presentation.explanation()

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.mediumSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                PlasmaComponents.Label {
                    text: KI18n.i18n("Daily focus")
                    color: Kirigami.Theme.disabledTextColor
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.capitalization: Font.AllUppercase
                }
                PlasmaExtras.Heading {
                    objectName: "dailyFocusHeadline"
                    Layout.fillWidth: true
                    level: 3
                    text: focus.presentation.headline()
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: focus.presentation.explanation()
                    wrapMode: Text.WordWrap
                    color: focus.actionRow.attentionSeverity
                           && focus.actionRow.attentionSeverity !== "none"
                        ? Kirigami.Theme.neutralTextColor
                        : Kirigami.Theme.textColor
                }
            }

            PlasmaComponents.Button {
                objectName: "dailyFocusAction"
                visible: !!focus.actionRow.stableId
                    && focus.presentation.actionLabel(focus.actionRow) !== ""
                text: focus.presentation.actionLabel(focus.actionRow)
                icon.name: focus.presentation.actionIcon(focus.actionRow)
                activeFocusOnTab: true
                Accessible.name: text + " · " + (focus.actionRow.displayName || "")
                onClicked: focus.actionRequested(
                    focus.actionRow.stableId,
                    focus.actionRow.nextActionKey,
                    focus.actionRow.sourceKind)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: focus.width < Kirigami.Units.gridUnit * 22 ? 1 : 3
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            visible: focus.facts.length > 0

            Repeater {
                model: focus.facts

                RowLayout {
                    id: fact
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: fact.modelData.icon
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: width
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        PlasmaComponents.Label {
                            text: fact.modelData.value
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        PlasmaComponents.Label {
                            text: fact.modelData.label
                            color: Kirigami.Theme.disabledTextColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }
}
