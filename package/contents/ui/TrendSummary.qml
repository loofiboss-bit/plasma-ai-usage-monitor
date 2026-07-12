import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

/**
 * Summary statistics for historical usage data.
 *
 * summaryData: QVariantMap with keys: totalCost, avgDailyCost, maxDailyCost,
 *              totalRequests, peakTokenUsage, snapshotCount
 * dailyCosts:  QVariantList of QVariantMap with keys: date, totalCost, maxDailyCost
 */
Item {
    id: trendRoot

    property var summaryData: ({})
    property var dailyCosts: []
    property string provider: ""
    property bool showEmptyState: true
    readonly property bool hasSummaryData: !!summaryData
        && Object.keys(summaryData).length > 0
        && (summaryData.snapshotCount || 0) > 0

    implicitHeight: hasSummaryData ? summaryGrid.implicitHeight : (showEmptyState ? emptyStateLabel.implicitHeight : 0)

    // Empty state
    PlasmaComponents.Label {
        id: emptyStateLabel
        anchors.centerIn: parent
        visible: trendRoot.showEmptyState && !trendRoot.hasSummaryData
        text: i18n("No historical data available")
        color: Kirigami.Theme.disabledTextColor
        font.pointSize: Kirigami.Theme.smallFont.pointSize
    }

    GridLayout {
        id: summaryGrid
        anchors.fill: parent
        visible: trendRoot.hasSummaryData
        columns: 2
        columnSpacing: Kirigami.Units.largeSpacing
        rowSpacing: Kirigami.Units.smallSpacing

        // ── Total Cost ──
        PlasmaComponents.Label {
            text: i18n("Total Cost:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
        }
        PlasmaComponents.Label {
            text: summaryData.mixedCurrencies
                ? Utils.formatCurrencyTotals(summaryData.currencyTotals || {})
                : Utils.formatCurrencyTotals(summaryData.currencyTotals || ({ USD: summaryData.totalCost || 0 }))
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // ── Average Daily Cost ──
        PlasmaComponents.Label {
            text: i18n("Avg Daily Cost:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
        }
        PlasmaComponents.Label {
            text: dailyCosts.length > 0
                ? Utils.formatMoney(summaryData.avgDailyCost || 0, dailyCosts[0].currency || "USD")
                : i18n("Not derivable")
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // ── Max Daily Cost ──
        PlasmaComponents.Label {
            text: i18n("Peak Daily Cost:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
        }
        PlasmaComponents.Label {
            text: dailyCosts.length > 0
                ? Utils.formatMoney(summaryData.maxDailyCost || 0, dailyCosts[0].currency || "USD")
                : i18n("Not derivable")
            color: (summaryData.maxDailyCost || 0) > (summaryData.avgDailyCost || 0) * 2
                   ? Kirigami.Theme.negativeTextColor
                   : Kirigami.Theme.textColor
        }

        // ── Total Requests ──
        PlasmaComponents.Label {
            text: i18n("Total Requests:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
        }
        PlasmaComponents.Label {
            text: formatNumber(summaryData.totalRequests || 0)
        }

        // ── Peak Token Usage ──
        PlasmaComponents.Label {
            text: i18n("Peak Tokens:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
        }
        PlasmaComponents.Label {
            text: formatNumber(summaryData.peakTokenUsage || 0)
        }

        // ── Data Points ──
        PlasmaComponents.Label {
            text: i18n("Data Points:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
        }
        PlasmaComponents.Label {
            text: (summaryData.snapshotCount || 0).toString()
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }

        // ── Trend indicator ──
        PlasmaComponents.Label {
            text: i18n("Trend:")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.alignment: Qt.AlignRight
            visible: dailyCosts && dailyCosts.length >= 3
        }
        RowLayout {
            visible: dailyCosts && dailyCosts.length >= 3
            spacing: Kirigami.Units.smallSpacing

            // Arrow indicating direction
            Kirigami.Icon {
                source: trendDirection() > 0 ? "arrow-up" : (trendDirection() < 0 ? "arrow-down" : "arrow-right")
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                color: trendDirection() > 0 ? Kirigami.Theme.negativeTextColor
                     : (trendDirection() < 0 ? Kirigami.Theme.positiveTextColor
                     : Kirigami.Theme.textColor)
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    var dir = trendDirection();
                    if (dir > 0) return i18n("Costs increasing");
                    if (dir < 0) return i18n("Costs decreasing");
                    return i18n("Costs stable");
                }
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: trendDirection() > 0 ? Kirigami.Theme.negativeTextColor
                     : (trendDirection() < 0 ? Kirigami.Theme.positiveTextColor
                     : Kirigami.Theme.textColor)
            }
        }
    }

    // ── Helper functions (delegated to Utils.js) ──

    function formatNumber(val) {
        return Utils.formatNumber(val);
    }

    /**
     * Simple trend detection: compare average of first half vs second half of daily costs.
     * Returns: 1 (increasing), -1 (decreasing), 0 (stable)
     */
    function trendDirection() {
        if (!dailyCosts || dailyCosts.length < 3) return 0;

        var mid = Math.floor(dailyCosts.length / 2);
        var firstHalfSum = 0;
        var secondHalfSum = 0;

        for (var i = 0; i < mid; i++) {
            firstHalfSum += (dailyCosts[i].totalCost || 0);
        }
        for (var j = mid; j < dailyCosts.length; j++) {
            secondHalfSum += (dailyCosts[j].totalCost || 0);
        }

        var firstAvg = firstHalfSum / mid;
        var secondAvg = secondHalfSum / (dailyCosts.length - mid);

        // 10% threshold for significance
        var threshold = Math.max(firstAvg, 0.001) * 0.1;
        if (secondAvg > firstAvg + threshold) return 1;
        if (secondAvg < firstAvg - threshold) return -1;
        return 0;
    }
}
