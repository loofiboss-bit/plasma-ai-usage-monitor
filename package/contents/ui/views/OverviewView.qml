pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import ".." as Monitor
import "../components" as Components
import "../Utils.js" as Utils

QQC2.ScrollView {
    id: overview

    property var monitor: null
    property string sourceSearchText: ""
    property int sourceFilterIndex: 0
    property bool costsExpanded: false
    property var viewState: ({})
    readonly property var visibleSourceRows: filterSourceRows()
    readonly property var visibleSourceGroups: {
        var favorites = visibleSourceRows.filter(function(row) {
            return overviewState.favoriteSourceIds.indexOf(row.stableId) >= 0;
        });
        var other = visibleSourceRows.filter(function(row) {
            return overviewState.favoriteSourceIds.indexOf(row.stableId) < 0;
        });
        return (favorites.length ? [{key: "favorites", label: i18n("Favorites"), rows: favorites}] : [])
            .concat(Utils.groupOverviewSourceRows(other));
    }
    signal sourceRequested(string stableId)
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    Accessible.role: Accessible.Pane
    Accessible.name: i18n("Overview view ready")

    Components.DailyOverviewState {
        id: overviewState
        objectName: "overviewState"
        dailyState: overview.monitor.presentationDailyState
        favoriteSourceIds: overview.monitor.favoriteSourceIds || []
        sessionActivity: overview.monitor.sessionActivity || null
    }

    function restoreSourceFocus(stableId) {
        for (var i = 0; i < sourceGroupRepeater.count; ++i) {
            // qmllint disable missing-property
            var group = sourceGroupRepeater.itemAt(i);
            if (group && group.restoreSourceFocus(stableId)) return true;
            // qmllint enable missing-property
        }
        // A removed favorite can leave the saved filter without that source.
        sourceSearch.forceActiveFocus(Qt.BacktabFocusReason);
        return false;
    }

    function captureViewState() {
        var contentY = 0;
        // Controls exposes a Flickable here at runtime; qmltypes declare QQuickItem.
        // qmllint disable missing-property
        if (overview.contentItem && overview.contentItem.contentY !== undefined)
            contentY = overview.contentItem.contentY;
        // qmllint enable missing-property
        return {
            searchText: sourceSearchText,
            filterIndex: sourceFilterIndex,
            costsExpanded: costsExpanded,
            contentY: contentY
        };
    }

    function applyViewState() {
        var saved = viewState || ({});
        sourceSearchText = saved.searchText || "";
        sourceFilterIndex = [1, 2].indexOf(saved.filterIndex) >= 0 ? saved.filterIndex : 0;
        costsExpanded = !!saved.costsExpanded;
        Qt.callLater(function() {
            // The QML type for ScrollView contentItem is generic; at runtime it is a Flickable.
            // qmllint disable missing-property
            if (!overview.contentItem || overview.contentItem.contentY === undefined)
                return;
            var maxY = Math.max(0, overview.contentItem.contentHeight
                - overview.contentItem.height);
            overview.contentItem.contentY = Math.max(0,
                Math.min(Number(saved.contentY || 0), maxY));
            // qmllint enable missing-property
        });
    }

    onViewStateChanged: applyViewState()

    function filterSourceRows() {
        var rows = sourceFilterIndex === 2 ? overviewState.sourceRows.filter(function(row) {
            return overviewState.favoriteSourceIds.indexOf(row.stableId) >= 0;
        }) : overviewState.sourceRows;
        return Utils.filterOverviewSourceRows(rows,
                                              sourceSearchText,
                                              sourceFilterIndex === 1);
    }

    function sessionActivityText(stableId) {
        return monitor && monitor.sessionActivity ? monitor.sessionActivity.activityText(stableId) : "";
    }

    ColumnLayout {
        width: overview.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        Components.DailyFocus {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            presentation: overviewState
            guardrails: overview.monitor.guardrails
            forecastsEnabled: Plasmoid.configuration.forecastUiEnabled
            onActionRequested: function(stableId, actionKey, sourceKind) {
                if (actionKey === "review_costs") overview.costsExpanded = true;
                else overview.actionRequested(stableId, actionKey, sourceKind);
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: costSummary.spendRows.length > 0

            PlasmaComponents.Button {
                objectName: "overviewMoreCostsToggle"
                text: i18n("More costs")
                icon.name: overview.costsExpanded ? "arrow-down" : "arrow-right"
                checkable: true
                checked: overview.costsExpanded
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: overview.costsExpanded = checked
            }
            Item { Layout.fillWidth: true }
        }

        Monitor.CostSummaryCard {
            id: costSummary
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            summary: overviewState.summary
            visible: overview.costsExpanded && spendRows.length > 0
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overview.monitor.modelMigrationNotice !== ""
            text: overview.monitor.modelMigrationNotice
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing

            PlasmaExtras.Heading {
                level: 4
                text: i18n("Sources")
                Layout.fillWidth: true
            }

            PlasmaComponents.Label {
                text: overview.sourceSearchText.trim() === ""
                        && overview.sourceFilterIndex === 0
                    ? i18np("%1 source", "%1 sources", overviewState.sourceRows.length)
                    : i18n("%1 shown · %2 total", overview.visibleSourceRows.length,
                           overviewState.sourceRows.length)
                color: Kirigami.Theme.disabledTextColor
                Accessible.name: i18np("%1 source", "%1 sources",
                                             overviewState.sourceRows.length)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.sourceRows.length === 0
            text: i18n("Enable a source or run guided setup to start monitoring.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            columns: overview.width >= Kirigami.Units.gridUnit * 30 ? 2 : 1
            visible: overviewState.sourceRows.length > 0

            Kirigami.SearchField {
                id: sourceSearch
                objectName: "overviewSourceSearch"
                Layout.fillWidth: true
                placeholderText: i18n("Search sources")
                Accessible.name: i18n("Search monitoring sources")
                text: overview.sourceSearchText
                onTextChanged: overview.sourceSearchText = text
            }

            QQC2.ComboBox {
                objectName: "overviewSourceFilter"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: [i18n("All"), i18n("Needs attention"), i18n("Favorites")]
                currentIndex: overview.sourceFilterIndex
                Accessible.name: i18n("Filter sources")
                onActivated: function(index) { overview.sourceFilterIndex = index; }
            }
        }

        RowLayout {
            objectName: "overviewQuickFilterBar"
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing
            visible: overviewState.sourceRows.length > 0

            PlasmaComponents.ToolButton {
                objectName: "overviewQuickFilterAll"
                text: i18n("All (%1)", overviewState.sourceRows.length)
                checkable: true
                checked: overview.sourceFilterIndex === 0
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: overview.sourceFilterIndex = 0
            }
            PlasmaComponents.ToolButton {
                objectName: "overviewQuickFilterFavorites"
                text: i18n("Favorites (%1)", overviewState.favoriteSourceIds.length)
                icon.name: "rating"
                checkable: true
                checked: overview.sourceFilterIndex === 2
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: overview.sourceFilterIndex = 2
            }
            PlasmaComponents.ToolButton {
                objectName: "overviewQuickFilterAttention"
                text: i18n("Needs attention (%1)", overviewState.attentionRows.length)
                icon.name: "dialog-warning"
                checkable: true
                checked: overview.sourceFilterIndex === 1
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: overview.sourceFilterIndex = 1
            }
            Item { Layout.fillWidth: true }
        }

        PlasmaComponents.Label {
            objectName: "overviewSourcesEmptyState"
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.sourceRows.length > 0
                && overview.visibleSourceRows.length === 0
            text: overview.sourceSearchText.trim() !== ""
                ? i18n("No source matches your search and current filter.")
                : overview.sourceFilterIndex === 1
                ? i18n("No sources currently need attention.")
                : overview.sourceFilterIndex === 2
                ? i18n("No enabled favorites. Add a favorite from a source card or source details.")
                : i18n("No source matches your search.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        PlasmaComponents.Button {
            objectName: "overviewClearSourceFilters"
            Layout.leftMargin: Kirigami.Units.largeSpacing
            visible: overviewState.sourceRows.length > 0
                && (overview.sourceSearchText !== "" || overview.sourceFilterIndex !== 0)
            text: i18n("Clear search and filters")
            onClicked: {
                overview.sourceSearchText = "";
                overview.sourceFilterIndex = 0;
                sourceSearch.forceActiveFocus(Qt.TabFocusReason);
            }
        }

        Repeater {
            id: sourceGroupRepeater
            model: overview.visibleSourceGroups

            delegate: ColumnLayout {
                id: sourceGroup
                required property var modelData
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaExtras.Heading {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    level: 4
                    text: i18n("%1 · %2", sourceGroup.modelData.label,
                               sourceGroup.modelData.rows.length)
                }

                Repeater {
                    id: sourceRowsRepeater
                    model: sourceGroup.modelData.rows

                    Monitor.DailySourceCard {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        Layout.rightMargin: Kirigami.Units.smallSpacing
                        row: modelData
                        favorite: overviewState.favoriteSourceIds.indexOf(modelData.stableId) >= 0
                        onFavoriteToggled: function(stableId) { overview.monitor.toggleFavorite(stableId); }
                        sessionActivityText: overview.sessionActivityText(modelData.stableId)
                        onActionRequested: function(stableId, actionKey, sourceKind) {
                            overview.actionRequested(stableId, actionKey, sourceKind);
                        }
                        onSourceRequested: function(stableId) {
                            overview.sourceRequested(stableId);
                        }
                    }
                }

                function restoreSourceFocus(stableId) {
                    for (var i = 0; i < sourceRowsRepeater.count; ++i) {
                        // qmllint disable missing-property
                        var card = sourceRowsRepeater.itemAt(i);
                        if (card && card.row.stableId === stableId) {
                            card.forceActiveFocus(Qt.BacktabFocusReason);
                            // qmllint enable missing-property
                            return true;
                        }
                        // qmllint enable missing-property
                    }
                    return false;
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }

    Component.onCompleted: {
        applyViewState();
    }
}
