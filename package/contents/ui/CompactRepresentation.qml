import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "components" as Components

MouseArea {
    id: compactRoot

    required property var monitor
    readonly property url brandedIconSource: Qt.resolvedUrl("../icons/logo.png")
    readonly property var providers: compactRoot.monitor.allProviders ?? []
    readonly property var subscriptionTools: compactRoot.monitor.allSubscriptionTools ?? []

    Components.CompactMetricState {
        id: compactState
        summary: compactRoot.monitor.presentationDailyState
                 && compactRoot.monitor.presentationDailyState.summary
            ? compactRoot.monitor.presentationDailyState.summary : ({})
    }

    readonly property string displayMode: compactState.normalizeMode(
        Plasmoid.configuration.compactDisplayMode)
    readonly property string statusKey: compactState.statusKey()
    readonly property bool anyLoading: {
        for (var i = 0; i < providers.length; i++) {
            if (providers[i] && providers[i].enabled && providers[i].backend
                    && providers[i].backend.loading) return true;
        }
        for (var j = 0; j < subscriptionTools.length; j++) {
            var tool = subscriptionTools[j];
            if (tool && tool.enabled && tool.monitor
                    && (tool.monitor.syncing || tool.monitor.syncInProgress)) return true;
        }
        return false;
    }

    Accessible.role: Accessible.Button
    Accessible.name: i18n("AI Usage Monitor: %1", accessibleText())
    hoverEnabled: true
    onClicked: Plasmoid.activated()

    Kirigami.Icon {
        id: mainIcon
        anchors.fill: parent
        source: compactRoot.brandedIconSource
        active: compactRoot.containsMouse
        visible: compactRoot.displayMode === "icon"

        Rectangle {
            visible: compactRoot.statusKey !== "hidden"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Kirigami.Units.smallSpacing * 3.5
            height: width
            radius: width / 2
            color: compactRoot.statusColor()
            border.width: 1
            border.color: Kirigami.Theme.backgroundColor
            Behavior on color { ColorAnimation { duration: 200 } }

            PlasmaComponents.Label {
                objectName: "compactSeveritySymbol"
                anchors.centerIn: parent
                visible: compactRoot.statusKey === "critical"
                    || compactRoot.statusKey === "warning"
                text: "!"
                color: Kirigami.Theme.backgroundColor
                font.bold: true
                font.pixelSize: Math.max(8, parent.height - 3)
                Accessible.ignored: true
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        visible: compactRoot.displayMode !== "icon"
        spacing: Kirigami.Units.smallSpacing / 2

        Kirigami.Icon {
            source: compactRoot.brandedIconSource
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: width
        }

        PlasmaComponents.Label {
            objectName: "compactDailyValue"
            Layout.fillWidth: true
            text: compactRoot.displayText()
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.bold: true
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            minimumPointSize: Math.max(6, Kirigami.Theme.smallFont.pointSize - 2)
            fontSizeMode: Text.Fit
            wrapMode: compactRoot.displayMode === "attention" ? Text.WordWrap : Text.NoWrap
            color: compactRoot.statusKey === "critical"
                ? Kirigami.Theme.negativeTextColor
                : compactRoot.statusKey === "warning"
                    ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
        }
    }

    PlasmaComponents.BusyIndicator {
        anchors.fill: parent
        visible: compactRoot.anyLoading
        running: visible
    }

    function statusColor() {
        if (statusKey === "critical") return Kirigami.Theme.negativeTextColor;
        if (statusKey === "warning") return Kirigami.Theme.neutralTextColor;
        if (statusKey === "healthy") return Kirigami.Theme.positiveTextColor;
        return Kirigami.Theme.disabledTextColor;
    }

    function displayText() {
        return compactState.displayText(displayMode);
    }

    function accessibleText() {
        if (displayMode === "icon") return compactState.summaryText();
        return displayText() + ". " + compactState.summaryText();
    }
}
