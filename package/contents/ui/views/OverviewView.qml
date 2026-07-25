pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import ".." as Monitor
import "../components" as Components

QQC2.ScrollView {
    id: overview
    property var monitor: null
    Accessible.role: Accessible.Pane
    Accessible.name: KI18n.i18n("Overview view ready")

    property bool connectionChecksExpanded: false
    readonly property var sourceSections: [
        { label: KI18n.i18n("Provider-reported data"), rows: overviewState.actualRows },
        { label: KI18n.i18n("Local estimates"), rows: overviewState.estimatedRows },
        { label: KI18n.i18n("Balances"), rows: overviewState.balanceRows },
        { label: KI18n.i18n("Unavailable"), rows: overviewState.unavailableRows }
    ]

    Components.DailyOverviewState {
        id: overviewState
        dailyState: overview.monitor.presentationDailyState
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
            text: KI18n.i18n("Sources")
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: Number(overviewState.summary.enabledSourceCount || 0) > 0
        }

        Repeater {
            model: overview.sourceSections

            ColumnLayout {
                id: sourceSection
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                visible: sourceSection.modelData.rows.length > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: sourceSection.modelData.label
                        font.bold: true
                    }
                    PlasmaComponents.Label {
                        text: sourceSection.modelData.rows.length
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                Repeater {
                    model: sourceSection.modelData.rows
                    Monitor.DailySourceCard {
                        required property var modelData
                        Layout.fillWidth: true
                        row: modelData
                        onActionRequested: function(stableId, actionKey, sourceKind) {
                            overview.monitor.fixOverviewSource(
                                stableId, actionKey, sourceKind);
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
                text: KI18n.i18n("Connectivity only")
                font.bold: true
                color: Kirigami.Theme.disabledTextColor
            }
            PlasmaComponents.Button {
                text: overview.connectionChecksExpanded ? KI18n.i18n("Hide")
                    : KI18n.i18np("Show %1 source", "Show %1 sources",
                             overviewState.connectivityRows.length)
                icon.name: overview.connectionChecksExpanded ? "arrow-up" : "arrow-down"
                checkable: true
                checked: overview.connectionChecksExpanded
                activeFocusOnTab: true
                Accessible.name: overview.connectionChecksExpanded
                    ? KI18n.i18n("Hide connectivity-only sources")
                    : KI18n.i18n("Show connectivity-only sources")
                onClicked: overview.connectionChecksExpanded = !overview.connectionChecksExpanded
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.connectivityRows.length > 0
                  && !overview.connectionChecksExpanded
            text: KI18n.i18n("These sources confirm endpoint access but do not report usage, spend, or live quota.")
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
