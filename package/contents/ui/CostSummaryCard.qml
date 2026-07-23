pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

Rectangle {
    id: card

    required property var summary
    readonly property var spendRows: buildRows()

    visible: spendRows.length > 0
    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.backgroundColor
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
    Accessible.role: Accessible.StaticText
    Accessible.name: i18n("Spend and budgets. %1", accessibleSummary())

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            level: 4
            text: i18n("Spend and budgets")
            Layout.fillWidth: true
        }

        Repeater {
            model: card.spendRows

            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: modelData.icon
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: width
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: modelData.label
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    text: Utils.formatCurrencyTotals(modelData.totals)
                    font.bold: true
                }
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: i18n("Actual spend, local estimates, and fixed subscription fees are never combined. Currencies are not converted.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
        }
    }

    function hasTotals(totals) {
        return totals && Object.keys(totals).length > 0;
    }

    function buildRows() {
        var rows = [];
        var actual = summary.actualSpendTotals || {};
        var estimated = summary.estimatedSpendTotals || {};
        var fees = summary.fixedSubscriptionFees || {};
        if (hasTotals(actual))
            rows.push({ label: i18n("Actual spend"), icon: "wallet-open", totals: actual });
        if (hasTotals(estimated))
            rows.push({ label: i18n("Estimated spend"), icon: "view-statistics", totals: estimated });
        if (hasTotals(fees))
            rows.push({ label: i18n("Fixed subscription fees"), icon: "office-chart-ring", totals: fees });
        return rows;
    }

    function accessibleSummary() {
        var parts = [];
        for (var i = 0; i < spendRows.length; i++)
            parts.push(spendRows[i].label + ": " + Utils.formatCurrencyTotals(spendRows[i].totals));
        return parts.join(i18n(" · "));
    }
}
