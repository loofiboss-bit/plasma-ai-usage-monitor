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

QQC2.ScrollView {
    id: overview

    property bool connectionChecksExpanded: false
    readonly property var sourceSections: [
        { label: i18n("Provider-reported data"), rows: overviewState.actualRows },
        { label: i18n("Local estimates"), rows: overviewState.estimatedRows },
        { label: i18n("Balances"), rows: overviewState.balanceRows },
        { label: i18n("Unavailable"), rows: overviewState.unavailableRows }
    ]

    Components.DailyOverviewState {
        id: overviewState
        dailyState: root.presentationDailyState
    }

    ColumnLayout {
        width: overview.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        Components.StatusHeader {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            presentation: overviewState
        }

        Components.AttentionList {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            presentation: overviewState
            onFixRequested: function(stableId, actionKey, sourceKind) {
                root.fixOverviewSource(stableId, actionKey, sourceKind);
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: root.modelMigrationNotice !== ""
            text: root.modelMigrationNotice
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }

        Monitor.QuotaResetCard {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            presentation: overviewState
        }

        Monitor.CostSummaryCard {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            summary: overviewState.summary
        }

        PlasmaExtras.Heading {
            level: 4
            text: i18n("Sources")
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: Number(overviewState.summary.enabledSourceCount || 0) > 0
        }

        Repeater {
            model: overview.sourceSections

            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                visible: modelData.rows.length > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: modelData.label
                        font.bold: true
                    }
                    PlasmaComponents.Label {
                        text: modelData.rows.length
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                Repeater {
                    model: modelData.rows
                    Monitor.DailySourceCard {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        onActionRequested: function(stableId, actionKey, sourceKind) {
                            root.fixOverviewSource(stableId, actionKey, sourceKind);
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: overviewState.connectivityRows.length > 0

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: i18n("Connectivity only")
                font.bold: true
                color: Kirigami.Theme.disabledTextColor
            }
            PlasmaComponents.Button {
                text: overview.connectionChecksExpanded ? i18n("Hide")
                    : i18np("Show %1 source", "Show %1 sources",
                             overviewState.connectivityRows.length)
                icon.name: overview.connectionChecksExpanded ? "arrow-up" : "arrow-down"
                checkable: true
                checked: overview.connectionChecksExpanded
                activeFocusOnTab: true
                Accessible.name: overview.connectionChecksExpanded
                    ? i18n("Hide connectivity-only sources")
                    : i18n("Show connectivity-only sources")
                onClicked: overview.connectionChecksExpanded = !overview.connectionChecksExpanded
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.connectivityRows.length > 0
                  && !overview.connectionChecksExpanded
            text: i18n("These sources confirm endpoint access but do not report usage, spend, or live quota.")
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: overview.connectionChecksExpanded ? overviewState.connectivityRows : []
            Monitor.DailySourceCard {
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                row: modelData
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }
}
