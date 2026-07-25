pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: card

    required property var presentation
    readonly property var quota: presentation.summary.lowestActualRemainingQuota || ({})
    readonly property var reset: presentation.summary.nearestActualReset || ({})

    visible: !!quota.stableId || !!reset.stableId
    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.backgroundColor
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
    Accessible.role: Accessible.StaticText
    Accessible.name: KI18n.i18n("Quota and resets. %1", detailText())

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            level: 4
            text: KI18n.i18n("Quota and resets")
            Layout.fillWidth: true
        }

        GridLayout {
            Layout.fillWidth: true
            columns: card.width < Kirigami.Units.gridUnit * 18 ? 1 : 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            ColumnLayout {
                Layout.fillWidth: true
                visible: !!card.quota.stableId
                spacing: 0
                PlasmaComponents.Label {
                    text: KI18n.i18n("Lowest live quota")
                    color: Kirigami.Theme.disabledTextColor
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                }
                PlasmaComponents.Label {
                    objectName: "lowestQuotaValue"
                    text: KI18n.i18n("%1% remaining", Math.round(Number(card.quota.percentRemaining)))
                    font.bold: true
                }
                PlasmaComponents.Label {
                    text: card.quota.displayName || ""
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !!card.reset.stableId
                spacing: 0
                PlasmaComponents.Label {
                    text: KI18n.i18n("Next live reset")
                    color: Kirigami.Theme.disabledTextColor
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                }
                PlasmaComponents.Label {
                    objectName: "nextResetValue"
                    text: card.presentation.relativeReset(card.reset.resetAt)
                    font.bold: true
                }
                PlasmaComponents.Label {
                    text: card.reset.displayName || ""
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: KI18n.i18n("Only synced or provider-reported quota windows appear here; published limits are excluded.")
            wrapMode: Text.WordWrap
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }
    }

    function detailText() {
        var parts = [];
        if (quota.stableId)
            parts.push(KI18n.i18n("%1 has %2% remaining", quota.displayName,
                            Math.round(Number(quota.percentRemaining))));
        if (reset.stableId)
            parts.push(KI18n.i18n("%1 resets in %2", reset.displayName,
                            presentation.relativeReset(reset.resetAt)));
        return parts.join(KI18n.i18n(" · "));
    }
}
