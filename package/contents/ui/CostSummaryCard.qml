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
                id: spendRow
                required property var modelData
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: spendRow.modelData.icon
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: width
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: spendRow.modelData.label
                    elide: Text.ElideRight
                }
                PlasmaComponents.Label {
                    text: card.rowValue(spendRow.modelData)
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
        var groups = summary.spendGroups || [];
        for (var g = 0; g < groups.length; ++g) {
            var group = groups[g] || {};
            if (group.quality !== "actual" && group.quality !== "estimated") continue;
            var label = group.quality === "estimated"
                ? i18n("Estimated spend · %1", Utils.spendWindowText(group))
                : i18n("Actual spend · %1", Utils.spendWindowText(group));
            rows.push({ label: label,
                icon: group.quality === "estimated" ? "view-statistics" : "wallet-open",
                group: group });
        }
        var fees = summary.fixedSubscriptionFees || {};
        if (hasTotals(fees))
            rows.push({ label: i18n("Fixed subscription fees"), icon: "office-chart-ring", totals: fees });
        var ranges = summary.fixedSubscriptionFeeRanges || [];
        for (var i = 0; i < ranges.length; ++i) {
            var range = ranges[i];
            rows.push({ label: i18n("Published fee range · %1", range.displayName),
                icon: "office-chart-ring", range: range });
        }
        return rows;
    }

    function rowValue(row) {
        if (row.group) return Utils.formatSpendGroup(row.group);
        if (!row.range) return Utils.formatCurrencyTotals(row.totals);
        return i18n("%1–%2 %3", Number(row.range.rangeMin).toFixed(2),
            Number(row.range.rangeMax).toFixed(2), row.range.currency);
    }

    function accessibleSummary() {
        var parts = [];
        for (var i = 0; i < spendRows.length; i++)
            parts.push(spendRows[i].label + ": " + rowValue(spendRows[i]));
        return parts.join(i18n(" · "));
    }
}
