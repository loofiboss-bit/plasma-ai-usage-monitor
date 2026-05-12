import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

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

    readonly property double apiSpend: {
        var total = 0;
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider.enabled && provider.backend && provider.backend.connected
                && (provider.backend.costSource === "billing_api" || provider.backend.costSource === "actual_api"))
                total += provider.backend.cost;
        }
        return total;
    }

    readonly property double apiSpendToday: {
        var total = 0;
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider.enabled && provider.backend && provider.backend.connected
                && (provider.backend.costSource === "billing_api" || provider.backend.costSource === "actual_api"))
                total += provider.backend.dailyCost;
        }
        return total;
    }

    readonly property double apiSpendThisMonth: {
        var total = 0;
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider.enabled && provider.backend && provider.backend.connected
                && (provider.backend.costSource === "billing_api" || provider.backend.costSource === "actual_api"))
                total += provider.backend.monthlyCost;
        }
        return total;
    }

    readonly property double estimatedBurn: {
        var total = 0;
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider.enabled && provider.backend && provider.backend.connected
                && (provider.backend.costSource === "estimated_from_usage" || provider.backend.isEstimatedCost))
                total += provider.backend.estimatedMonthlyCost || provider.backend.monthlyCost || provider.backend.cost || 0;
        }
        return total;
    }

    readonly property double totalMonthlyExposure: apiSpendThisMonth + subscriptionFees + estimatedBurn
    readonly property double totalCost: totalMonthlyExposure

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
                        var val = costCard.costViewMode === 0 ? costCard.totalCost
                                : costCard.costViewMode === 1 ? costCard.apiSpendToday
                                : costCard.costViewMode === 2 ? costCard.apiSpendThisMonth
                                : costCard.costViewMode === 3 ? costCard.subscriptionFees
                                : costCard.estimatedBurn;
                        return "$" + val.toFixed(val < 1 ? 4 : 2);
                    }
                    font.bold: true
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                    horizontalAlignment: costCard.narrowCard ? Text.AlignLeft : Text.AlignRight
                    color: {
                        var cost = costCard.costViewMode === 0 ? costCard.totalCost
                                 : costCard.costViewMode === 1 ? costCard.apiSpendToday
                                 : costCard.costViewMode === 2 ? costCard.apiSpendThisMonth
                                 : costCard.costViewMode === 3 ? costCard.subscriptionFees
                                 : costCard.estimatedBurn;
                        if (cost > 50) return Kirigami.Theme.negativeTextColor;
                        if (cost > 20) return Kirigami.Theme.neutralTextColor;
                        return Kirigami.Theme.textColor;
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("API spend, fixed subscription fees, and estimated burn are labeled separately. Monthly exposure: $%1", costCard.totalMonthlyExposure.toFixed(2))
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
                        if (source !== "billing_api" && source !== "actual_api") return 0;
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
                        text: "$" + parent.providerCost.toFixed(parent.providerCost < 1 ? 4 : 2)
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
                        text: "$" + parent.toolCost.toFixed(parent.toolCost < 1 ? 4 : 2)
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        font.bold: true
                    }
                }
            }
        }
    }
}
