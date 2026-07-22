import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils
import "components" as Components

MouseArea {
    id: compactRoot
    readonly property url brandedIconSource: Qt.resolvedUrl("../icons/logo.png")

    readonly property var providers: root.allProviders ?? []
    readonly property var subscriptionTools: root.allSubscriptionTools ?? []
    readonly property int warningThreshold: plasmoid.configuration.warningThreshold || 80
    readonly property int criticalThreshold: plasmoid.configuration.criticalThreshold || 95

    Components.OverviewState {
        id: compactOverviewState
        providers: compactRoot.providers
        tools: compactRoot.subscriptionTools
        readinessModel: root.sourceReadiness
    }

    readonly property var compactApiSpend: Utils.actualCostTotals(providers, "current")

    readonly property double compactSubscriptionFees: {
        var total = 0;
        for (var j = 0; j < subscriptionTools.length; j++) {
            var tool = subscriptionTools[j];
            if (tool && tool.enabled && tool.monitor && tool.monitor.hasSubscriptionCost)
                total += tool.monitor.subscriptionCost ?? 0;
        }
        return total;
    }

    Accessible.role: Accessible.Button
    Accessible.name: i18n("AI Usage Monitor: %1", compactOverviewState.summaryText())

    readonly property string statusKey: Utils.compactStatus(
        compactOverviewState.summary, providers, subscriptionTools,
        compactRoot.warningThreshold, compactRoot.criticalThreshold)
    readonly property bool hasWarning: statusKey === "warning"
    readonly property bool hasCritical: statusKey === "critical"
    readonly property bool hasVerifiedSource: compactOverviewState.summary.verified > 0
    readonly property bool anyLoading: {
        if (compactOverviewState.summary.verifying > 0) return true;
        for (var i = 0; i < providers.length; i++) {
            if (providers[i] && providers[i].enabled && providers[i].backend && providers[i].backend.loading)
                return true;
        }
        for (var j = 0; j < subscriptionTools.length; j++) {
            var tool = subscriptionTools[j];
            if (tool && tool.enabled && tool.monitor
                    && (tool.monitor.syncing || tool.monitor.syncInProgress)) return true;
        }
        return false;
    }

    readonly property string displayMode: {
        var mode = plasmoid.configuration.compactDisplayMode;
        return ["cost", "count", "dailycost", "requests", "critical"].indexOf(mode) >= 0 ? mode : "icon";
    }

    hoverEnabled: true
    onClicked: plasmoid.activated()

    // Icon mode (default)
    Kirigami.Icon {
        id: mainIcon
        anchors.fill: parent
        source: compactRoot.brandedIconSource
        active: compactRoot.containsMouse
        visible: compactRoot.displayMode === "icon"

        // Overlay badge for status indication
        Rectangle {
            id: statusBadge
            visible: compactRoot.statusKey !== "hidden"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Kirigami.Units.smallSpacing * 3
            height: width
            radius: width / 2
            color: {
                if (compactRoot.statusKey === "critical") return Kirigami.Theme.negativeTextColor;
                if (compactRoot.statusKey === "warning") return Kirigami.Theme.neutralTextColor;
                if (compactRoot.statusKey === "healthy") return Kirigami.Theme.positiveTextColor;
                return Kirigami.Theme.disabledTextColor;
            }
            border.width: 1
            border.color: Kirigami.Theme.backgroundColor

            Behavior on color {
                ColorAnimation { duration: 300 }
            }
        }
    }

    // Cost mode
    PlasmaComponents.Label {
        id: costLabel
        anchors.fill: parent
        visible: compactRoot.displayMode === "cost"
        text: Utils.formatCurrencyTotals(compactRoot.compactApiSpend)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: true
        font.pointSize: Math.max(Kirigami.Theme.smallFont.pointSize, height * 0.35)
        minimumPointSize: Kirigami.Theme.smallFont.pointSize
        fontSizeMode: Text.Fit
        color: {
            var currencies = Object.keys(compactRoot.compactApiSpend);
            var cost = currencies.length === 1 ? compactRoot.compactApiSpend[currencies[0]] : 0;
            if (cost > 10) return Kirigami.Theme.negativeTextColor;
            if (cost > 5) return Kirigami.Theme.neutralTextColor;
            return Kirigami.Theme.textColor;
        }
    }

    // Count mode
    RowLayout {
        anchors.fill: parent
        visible: compactRoot.displayMode === "count"
        spacing: Kirigami.Units.smallSpacing / 2

        Kirigami.Icon {
            source: compactRoot.brandedIconSource
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
        }

        PlasmaComponents.Label {
            text: compactOverviewState.summary.useful.toString()
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }
    }

    readonly property var compactDailyCost: Utils.actualCostTotals(providers, "day")

    readonly property var requestAvailability: Utils.requestsRemaining(providers)

    readonly property string criticalSourceText: {
        var worst = Utils.worstQuotaSource(providers, subscriptionTools);
        if (worst.available)
            return worst.name + " (" + Math.round(worst.percent) + "%)";
        if (compactRoot.hasVerifiedSource && compactOverviewState.summary.attention === 0)
            return i18n("All healthy");
        return "\u2014";
    }

    // Daily cost mode
    PlasmaComponents.Label {
        anchors.fill: parent
        visible: compactRoot.displayMode === "dailycost"
        text: Utils.formatCurrencyTotals(compactRoot.compactDailyCost)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: true
        font.pointSize: Math.max(Kirigami.Theme.smallFont.pointSize, height * 0.35)
        fontSizeMode: Text.Fit
    }

    // Requests mode
    PlasmaComponents.Label {
        anchors.fill: parent
        visible: compactRoot.displayMode === "requests"
        text: compactRoot.requestAvailability.available
            ? i18n("%1 req", compactRoot.requestAvailability.value) : "\u2014"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: true
        font.pointSize: Math.max(Kirigami.Theme.smallFont.pointSize, height * 0.35)
        fontSizeMode: Text.Fit
    }

    // Critical mode
    PlasmaComponents.Label {
        anchors.fill: parent
        visible: compactRoot.displayMode === "critical"
        text: compactRoot.criticalSourceText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: true
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        fontSizeMode: Text.Fit
        wrapMode: Text.WordWrap
    }

    // Spinning indicator when loading (all modes)
    PlasmaComponents.BusyIndicator {
        anchors.fill: parent
        visible: compactRoot.anyLoading
        running: visible
    }

    function formatMetric(value) {
        if (value >= 1000000)
            return (value / 1000000).toFixed(1) + "M";
        if (value >= 1000)
            return (value / 1000).toFixed(1) + "K";
        return value.toString();
    }
}
