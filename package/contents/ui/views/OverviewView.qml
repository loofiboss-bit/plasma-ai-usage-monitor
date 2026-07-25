pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.ki18n
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import ".." as Monitor
import "../components" as Components

QQC2.ScrollView {
    id: overview

    property var monitor: null
    signal sourceRequested(string stableId)
    Accessible.role: Accessible.Pane
    Accessible.name: KI18n.i18n("Overview view ready")

    Components.DailyOverviewState {
        id: overviewState
        dailyState: overview.monitor.presentationDailyState
    }

    function restoreSourceFocus(stableId) {
        for (var i = 0; i < sourceRepeater.count; ++i) {
            // qmllint disable missing-property
            var card = sourceRepeater.itemAt(i);
            if (card && card.row.stableId === stableId) {
                card.forceActiveFocus(Qt.BacktabFocusReason);
                // qmllint enable missing-property
                return true;
            }
            // qmllint enable missing-property
        }
        return false;
    }

    ColumnLayout {
        width: overview.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        Components.DailyFocus {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            presentation: overviewState
            onActionRequested: function(stableId, actionKey, sourceKind) {
                overview.monitor.fixOverviewSource(stableId, actionKey,
                                                    sourceKind);
            }
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
                text: KI18n.i18n("Sources")
                Layout.fillWidth: true
            }
            PlasmaComponents.Label {
                text: overviewState.sourceRows.length
                color: Kirigami.Theme.disabledTextColor
                Accessible.name: KI18n.i18np("%1 source", "%1 sources",
                                             overviewState.sourceRows.length)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.sourceRows.length === 0
            text: KI18n.i18n("Enable a source or run guided setup to start monitoring.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        Repeater {
            id: sourceRepeater
            model: overviewState.sourceRows

            Monitor.DailySourceCard {
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                row: modelData
                onSourceRequested: function(stableId) {
                    overview.sourceRequested(stableId);
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }
}
