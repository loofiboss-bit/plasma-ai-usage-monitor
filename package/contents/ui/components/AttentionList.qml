pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: attention

    required property var presentation
    readonly property var row: presentation.topAction
    signal fixRequested(string stableId, string actionKey, string sourceKind)

    visible: !!row.stableId
    implicitHeight: actionRow.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Qt.alpha(row.attentionSeverity === "critical"
                    ? Kirigami.Theme.negativeBackgroundColor
                    : Kirigami.Theme.neutralBackgroundColor, 0.35)
    border.width: 1
    border.color: Qt.alpha(row.attentionSeverity === "critical"
                           ? Kirigami.Theme.negativeTextColor
                           : Kirigami.Theme.neutralTextColor, 0.28)

    RowLayout {
        id: actionRow
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        Kirigami.Icon {
            source: attention.row.attentionSeverity === "critical"
                ? "dialog-error" : "dialog-warning"
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: width
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            PlasmaExtras.Heading {
                objectName: "topActionTitle"
                level: 4
                Layout.fillWidth: true
                text: attention.row.displayName || ""
                elide: Text.ElideRight
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: attention.presentation.actionText(attention.row)
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.neutralTextColor
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: Number(attention.presentation.summary.attentionSourceCount || 0) > 1
                text: KI18n.i18np("%1 other source also needs attention",
                            "%1 other sources also need attention",
                            Number(attention.presentation.summary.attentionSourceCount) - 1)
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
            }
        }

        PlasmaComponents.Button {
            objectName: "topActionButton"
            text: attention.presentation.actionLabel(attention.row)
            icon.name: attention.row.nextActionKey === "refresh_stale_data" ? "view-refresh" : "configure"
            activeFocusOnTab: true
            Accessible.name: KI18n.i18n("%1 for %2: %3", text,
                                  attention.row.displayName,
                                  attention.presentation.actionText(attention.row))
            onClicked: attention.fixRequested(attention.row.stableId,
                                              attention.row.nextActionKey,
                                              attention.row.sourceKind)
        }
    }
}
