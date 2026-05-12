import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

MouseArea {
    id: compactRoot
    readonly property url brandedIconSource: Qt.resolvedUrl("../icons/logo.png")

    readonly property var providers: root.allProviders ?? []
    readonly property var subscriptionTools: root.allSubscriptionTools ?? []
    readonly property int warningThreshold: plasmoid.configuration.warningThreshold || 80
    readonly property int criticalThreshold: plasmoid.configuration.criticalThreshold || 95

    readonly property double compactApiSpend: {
        var total = 0;
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider && provider.enabled && provider.backend && provider.backend.connected
                && (provider.backend.costSource === "billing_api" || provider.backend.costSource === "actual_api"))
                total += provider.backend.cost ?? 0;
        }
        return total;
    }

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
    Accessible.name: i18n("AI Usage Monitor: %1 providers connected", root.connectedCount ?? 0)

    readonly property bool hasWarning: {
        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (p && p.enabled && p.backend && p.backend.connected && p.backend.rateLimitRequests > 0) {
                var usedPercent = ((p.backend.rateLimitRequests - p.backend.rateLimitRequestsRemaining) / p.backend.rateLimitRequests) * 100;
                if (usedPercent >= compactRoot.warningThreshold) return true;
            }
        }
        // Also check subscription tools
        var tools = root.allSubscriptionTools ?? [];
        for (var j = 0; j < tools.length; j++) {
            var t = tools[j];
            if (t && t.enabled && t.monitor && t.monitor.percentUsed >= compactRoot.warningThreshold) return true;
        }
        return false;
    }
    readonly property bool hasCritical: {
        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (p && p.enabled && p.backend && p.backend.connected && p.backend.rateLimitRequests > 0) {
                var usedPercent = ((p.backend.rateLimitRequests - p.backend.rateLimitRequestsRemaining) / p.backend.rateLimitRequests) * 100;
                if (usedPercent >= compactRoot.criticalThreshold) return true;
            }
        }
        // Also check subscription tools
        var tools = root.allSubscriptionTools ?? [];
        for (var j = 0; j < tools.length; j++) {
            var t = tools[j];
            if (t && t.enabled && t.monitor && (t.monitor.limitReached || t.monitor.percentUsed >= compactRoot.criticalThreshold)) return true;
        }
        return false;
    }
    readonly property bool anyConnected: {
        for (var i = 0; i < providers.length; i++) {
            if (providers[i] && providers[i].enabled && providers[i].backend && providers[i].backend.connected)
                return true;
        }
        return false;
    }
    readonly property bool anyLoading: {
        for (var i = 0; i < providers.length; i++) {
            if (providers[i] && providers[i].enabled && providers[i].backend && providers[i].backend.loading)
                return true;
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
            visible: compactRoot.anyConnected
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Kirigami.Units.smallSpacing * 3
            height: width
            radius: width / 2
            color: {
                if (compactRoot.hasCritical) return Kirigami.Theme.negativeTextColor;
                if (compactRoot.hasWarning) return Kirigami.Theme.neutralTextColor;
                return Kirigami.Theme.positiveTextColor;
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
        text: "$" + compactRoot.compactApiSpend.toFixed(2)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: true
        font.pointSize: Math.max(Kirigami.Theme.smallFont.pointSize, height * 0.35)
        minimumPointSize: Kirigami.Theme.smallFont.pointSize
        fontSizeMode: Text.Fit
        color: {
            var cost = compactRoot.compactApiSpend;
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
            text: (root.connectedCount ?? 0).toString()
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }
    }

    readonly property double compactDailyCost: {
        var total = 0;
        for (var i = 0; i < providers.length; i++) {
            var provider = providers[i];
            if (provider && provider.enabled && provider.backend && provider.backend.connected)
                total += provider.backend.dailyCost ?? 0;
        }
        return total;
    }

    readonly property int totalRequestsRemaining: {
        var req = 0;
        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (p && p.enabled && p.backend && p.backend.connected && p.backend.rateLimitRequestsRemaining > 0)
                req += p.backend.rateLimitRequestsRemaining;
        }
        return req;
    }

    readonly property string criticalProviderText: {
        var worst = "";
        var worstPercent = 0;
        for (var i = 0; i < providers.length; i++) {
            var p = providers[i];
            if (p && p.enabled && p.backend && p.backend.connected && p.backend.rateLimitRequests > 0) {
                var pct = ((p.backend.rateLimitRequests - p.backend.rateLimitRequestsRemaining) / p.backend.rateLimitRequests) * 100;
                if (pct > worstPercent) {
                    worstPercent = pct;
                    worst = p.name;
                }
            }
        }
        return worst !== "" ? worst + " (" + Math.round(worstPercent) + "%)" : i18n("All healthy");
    }

    // Daily cost mode
    PlasmaComponents.Label {
        anchors.fill: parent
        visible: compactRoot.displayMode === "dailycost"
        text: "$" + compactRoot.compactDailyCost.toFixed(2)
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
        text: compactRoot.totalRequestsRemaining + " req"
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
        text: compactRoot.criticalProviderText
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
