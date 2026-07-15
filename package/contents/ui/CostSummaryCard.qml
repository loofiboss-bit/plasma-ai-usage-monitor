import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

ColumnLayout {
    id: costCard

    property var providers: []
    property var subscriptionTools: []
    readonly property bool narrowCard: costCard.width < Kirigami.Units.gridUnit * 14

    readonly property double subscriptionFees: {
        var total = 0;
        for (var i = 0; i < subscriptionTools.length; i++) {
            var tool = subscriptionTools[i];
            if (tool.enabled && tool.monitor && tool.monitor.hasSubscriptionCost)
                total += tool.monitor.subscriptionCost;
        }
        return total;
    }

    function providerTotals(field, estimated) {
        var totals = {};
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (!provider.enabled || !provider.backend || !provider.backend.connected) continue;
            var source = provider.backend.costSource || "unknown";
            var isEstimate = source === "estimated_from_usage" || provider.backend.isEstimatedCost;
            if (estimated !== isEstimate) continue;
            if (!estimated && source !== "billing_api" && source !== "usage_api" && source !== "actual_api") continue;
            Utils.addCurrencyTotal(totals, provider.backend.currency, provider.backend[field] || 0);
        }
        return totals;
    }

    function mergedTotals() {
        var totals = {};
        var actual = apiSpendThisMonth;
        var estimates = estimatedBurn;
        var currencies = Object.keys(actual);
        for (var i = 0; i < currencies.length; i++) Utils.addCurrencyTotal(totals, currencies[i], actual[currencies[i]]);
        currencies = Object.keys(estimates);
        for (var j = 0; j < currencies.length; j++) Utils.addCurrencyTotal(totals, currencies[j], estimates[currencies[j]]);
        Utils.addCurrencyTotal(totals, "USD", subscriptionFees);
        return totals;
    }

    function selectedTotals() {
        if (costViewMode === 0) return totalMonthlyExposure;
        if (costViewMode === 1) return apiSpendToday;
        if (costViewMode === 2) return apiSpendThisMonth;
        if (costViewMode === 3) return ({ USD: subscriptionFees });
        return estimatedBurn;
    }

    function onlyValue(totals) {
        var keys = Object.keys(totals || {});
        return keys.length === 1 ? totals[keys[0]] : 0;
    }

    readonly property var apiSpend: providerTotals("cost", false)
    readonly property var apiSpendToday: providerTotals("dailyCost", false)
    readonly property var apiSpendThisMonth: providerTotals("monthlyCost", false)
    readonly property var estimatedBurn: {
        var totals = {};
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (!provider.enabled || !provider.backend || !provider.backend.connected) continue;
            var source = provider.backend.costSource || "unknown";
            if (source === "estimated_from_usage" || provider.backend.isEstimatedCost) {
                Utils.addCurrencyTotal(totals, provider.backend.currency,
                    provider.backend.estimatedMonthlyCost || provider.backend.monthlyCost || provider.backend.cost || 0);
            }
        }
        return totals;
    }
    readonly property var totalMonthlyExposure: mergedTotals()
    readonly property var totalCost: totalMonthlyExposure

    spacing: 0
    property int costViewMode: 0

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: costContent.implicitHeight + Kirigami.Units.largeSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: Kirigami.Units.cornerRadius
            color: Kirigami.Theme.highlightColor
            opacity: 0.6
        }

        ColumnLayout {
            id: costContent
            anchors {
                fill: parent
                margins: Kirigami.Units.largeSpacing
                leftMargin: Kirigami.Units.largeSpacing + 4
            }
            spacing: Kirigami.Units.mediumSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaExtras.Heading {
                        level: 4
                        text: costCard.costViewMode === 0 ? i18n("Total Monthly Exposure")
                            : costCard.costViewMode === 1 ? i18n("API Spend Today")
                            : costCard.costViewMode === 2 ? i18n("API Spend This Month")
                            : costCard.costViewMode === 3 ? i18n("Subscription Fees / Month")
                            : i18n("Estimated Monthly Burn")
                        Layout.fillWidth: true
                    }

                    Row {
                        spacing: 2
                        Repeater {
                            model: [
                                { label: i18n("Exposure"), mode: 0 },
                                { label: i18n("Day"), mode: 1 },
                                { label: i18n("Month"), mode: 2 },
                                { label: i18n("Fees"), mode: 3 },
                                { label: i18n("Burn"), mode: 4 }
                            ]

                            PlasmaComponents.ToolButton {
                                text: modelData.label
                                checked: costCard.costViewMode === modelData.mode
                                onClicked: costCard.costViewMode = modelData.mode
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                implicitHeight: Kirigami.Units.gridUnit * 1.2
                            }
                        }
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: {
                        return Utils.formatCurrencyTotals(costCard.selectedTotals());
                    }
                    font.bold: true
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                    horizontalAlignment: costCard.narrowCard ? Text.AlignLeft : Text.AlignRight
                    color: {
                        var cost = costCard.onlyValue(costCard.selectedTotals());
                        if (cost > 50) return Kirigami.Theme.negativeTextColor;
                        if (cost > 20) return Kirigami.Theme.neutralTextColor;
                        return Kirigami.Theme.textColor;
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: Object.keys(costCard.totalMonthlyExposure).length > 1
                        ? i18n("Mixed currencies are grouped and are never converted or silently summed: %1", Utils.formatCurrencyTotals(costCard.totalMonthlyExposure))
                        : i18n("API spend, fixed subscription fees, and estimated burn are labeled separately.")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.68
                    wrapMode: Text.WordWrap
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            Repeater {
                model: costCard.providers
                RowLayout {
                    Layout.fillWidth: true
                    readonly property double providerCost: {
                        if (!modelData.backend) return 0;
                        var source = modelData.backend.costSource || "unknown";
                        if (costCard.costViewMode === 3) return 0;
                        if (costCard.costViewMode === 4) {
                            if (source === "estimated_from_usage" || modelData.backend.isEstimatedCost)
                                return modelData.backend.estimatedMonthlyCost || modelData.backend.monthlyCost || modelData.backend.cost || 0;
                            return 0;
                        }
                        if (source !== "billing_api" && source !== "usage_api" && source !== "actual_api") return 0;
                        if (costCard.costViewMode === 1) return modelData.backend.dailyCost ?? 0;
                        if (costCard.costViewMode === 2) return modelData.backend.monthlyCost ?? 0;
                        return modelData.backend.monthlyCost || modelData.backend.cost || 0;
                    }
                    visible: modelData.enabled && modelData.backend && modelData.backend.connected && providerCost > 0
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle { width: 8; height: 8; radius: 4; color: modelData.color }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: modelData.name
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.8
                    }
                    PlasmaComponents.Label {
                        text: Utils.formatMoney(parent.providerCost, modelData.backend?.currency || "USD")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        font.bold: true
                    }
                }
            }

            Repeater {
                model: costCard.subscriptionTools
                RowLayout {
                    Layout.fillWidth: true
                    readonly property double toolCost: {
                        if (!modelData.monitor || !modelData.monitor.hasSubscriptionCost) return 0;
                        return modelData.monitor.subscriptionCost ?? 0;
                    }
                    visible: (costCard.costViewMode === 0 || costCard.costViewMode === 3)
                             && modelData.enabled && toolCost > 0
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle { width: 8; height: 8; radius: 4; color: modelData.monitor?.toolColor ?? Kirigami.Theme.highlightColor }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: i18n("%1 (subscription fee)", modelData.name)
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.8
                    }
                    PlasmaComponents.Label {
                        text: Utils.formatMoney(parent.toolCost, "USD")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        font.bold: true
                    }
                }
            }
        }
    }
}
