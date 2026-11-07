import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "components" as Components

MouseArea {
    id: compactRoot

    readonly property real textModeWidth: compactModeIcon.Layout.preferredWidth
        + compactContent.columnSpacing + Math.min(compactDailyValue.implicitWidth, Kirigami.Units.gridUnit * 12)
    readonly property bool verticalPanel: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    implicitWidth: displayMode === "icon"
        ? Math.max(Kirigami.Units.iconSizes.small, height) : verticalPanel ? Kirigami.Units.gridUnit * 5 : textModeWidth
    implicitHeight: verticalPanel && displayMode !== "icon"
        ? compactModeIcon.Layout.preferredHeight + compactContent.rowSpacing
            + compactDailyValue.implicitHeight
        : Kirigami.Units.iconSizes.small
    Layout.fillWidth: verticalPanel
    Layout.minimumWidth: verticalPanel ? Kirigami.Units.iconSizes.small : implicitWidth
    Layout.preferredWidth: implicitWidth
    Layout.maximumWidth: verticalPanel ? Kirigami.Units.gridUnit * 5 : implicitWidth
    Layout.minimumHeight: verticalPanel ? implicitHeight : Kirigami.Units.iconSizes.small
    Layout.preferredHeight: implicitHeight

    required property var monitor
    readonly property url brandedIconSource: Qt.resolvedUrl("../icons/logo.png")
    readonly property var providers: compactRoot.monitor.allProviders ?? []
    readonly property var subscriptionTools: compactRoot.monitor.allSubscriptionTools ?? []
    property int favoriteCycleIndex: -1

    readonly property var panelSourceIds: {
        var scope = Plasmoid.configuration.panelSourceScope || "automatic";
        if (scope === "all") return null;
        if (scope === "source") return [Plasmoid.configuration.panelSourceId || ""];
        var favorites = compactRoot.monitor.favoriteSourceIds || [];
        if (favoriteCycleIndex >= 0 && favoriteCycleIndex < favorites.length)
            return [favorites[favoriteCycleIndex]];
        return favorites.length ? favorites : null;
    }
    readonly property var scopedSummary: {
        var daily = monitor.presentationDailyState;
        var revision = daily ? daily.summary : ({});
        if (panelSourceIds === null) return revision;
        return daily && typeof daily.summaryForSources === "function"
            ? daily.summaryForSources(panelSourceIds) : ({});
    }
    Components.CompactMetricState {
        id: compactState
        presentationTime: compactRoot.monitor.dailyState.presentationTime
        summary: compactRoot.scopedSummary
        globalSummary: compactRoot.monitor.presentationDailyState
            ? compactRoot.monitor.presentationDailyState.summary : ({})
        activityTotals: compactRoot.monitor.sessionActivity
            ? compactRoot.monitor.sessionActivity.scopedTotals(compactRoot.panelSourceIds) : ({available: false})
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
    onWheel: function(wheel) {
        var scope = Plasmoid.configuration.panelSourceScope || "automatic";
        var favorites = compactRoot.monitor.favoriteSourceIds || [];
        if (scope !== "automatic" || favorites.length < 2) return;
        var step = wheel.angleDelta.y < 0 ? 1 : -1;
        var next = compactRoot.favoriteCycleIndex + step;
        if (next >= favorites.length) next = -1;
        else if (next < -1) next = favorites.length - 1;
        compactRoot.favoriteCycleIndex = next;
        wheel.accepted = true;
    }
    activeFocusOnTab: true
    Keys.onSpacePressed: Plasmoid.activated()
    Keys.onReturnPressed: Plasmoid.activated()

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

    GridLayout {
        id: compactContent
        anchors.fill: parent
        visible: compactRoot.displayMode !== "icon"
        columns: compactRoot.verticalPanel ? 1 : 2
        columnSpacing: Kirigami.Units.smallSpacing / 2
        rowSpacing: Kirigami.Units.smallSpacing / 2

        Kirigami.Icon {
            id: compactModeIcon
            source: compactRoot.brandedIconSource
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: width
            Layout.alignment: Qt.AlignHCenter
            PlasmaComponents.Label {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: compactRoot.statusKey === "critical" || compactRoot.statusKey === "warning"
                text: "!"
                font.bold: true
                color: compactRoot.statusColor()
                Accessible.ignored: true
            }
        }

        PlasmaComponents.Label {
            id: compactDailyValue
            objectName: "compactDailyValue"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            elide: Text.ElideRight
            clip: true
            text: compactRoot.displayText()
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.bold: true
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.NoWrap
            color: compactRoot.statusKey === "critical"
                ? Kirigami.Theme.negativeTextColor
                : compactRoot.statusKey === "warning"
                    ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
            PlasmaComponents.ToolTip {
                text: compactRoot.accessibleText()
            }
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

    function selectedSourceName() {
        var daily = monitor.presentationDailyState;
        var row = daily && typeof daily.source === "function"
            ? daily.source(Plasmoid.configuration.panelSourceId) : ({});
        return row.displayName || i18n("Selected source unavailable");
    }

    function accessibleText() {
        if (displayMode === "icon") return compactState.summaryText();
        return displayText() + ". " + compactState.summaryText()
            + ". " + (Plasmoid.configuration.panelSourceScope === "source"
                ? i18n("Source: %1", selectedSourceName())
                : panelSourceIds !== null ? i18n("Favorites") : i18n("All sources"));
    }
}
