pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: list

    required property var rows
    signal fixRequested(string stableId, string actionKey, string sourceKindKey)

    spacing: Kirigami.Units.smallSpacing
    visible: rows.length > 0

    Repeater {
        model: list.rows

        Rectangle {
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: attentionRow.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: Kirigami.Units.smallSpacing
            color: Qt.alpha(Kirigami.Theme.neutralBackgroundColor, 0.35)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.25)

            RowLayout {
                id: attentionRow
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: modelData.readinessStateKey === "failed" ? "dialog-error" : "dialog-warning"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: width
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: modelData.displayName
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: modelData.nextActionText || i18n("Review this source")
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.neutralTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }
                }

                PlasmaComponents.Button {
                    text: i18n("Fix")
                    icon.name: modelData.nextActionKey === "refresh_stale_data" ? "view-refresh" : "configure"
                    activeFocusOnTab: true
                    Accessible.name: i18n("Fix %1: %2", modelData.displayName,
                                          modelData.nextActionText || i18n("Review this source"))
                    onClicked: list.fixRequested(modelData.stableId, modelData.nextActionKey,
                                                 modelData.sourceKindKey)
                }
            }
        }
    }
}
