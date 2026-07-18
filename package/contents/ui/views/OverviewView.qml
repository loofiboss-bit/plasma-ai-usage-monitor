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

    readonly property var providers: root.allProviders || []
    readonly property var tools: root.allSubscriptionTools || []
    property bool connectionChecksExpanded: false

    Components.OverviewState {
        id: overviewState
        providers: overview.providers
        tools: overview.tools
        readinessModel: root.sourceReadiness
    }

    ColumnLayout {
        width: overview.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        Components.StatusHeader {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            summary: overviewState.summary
        }

        Components.AttentionList {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            rows: overviewState.attentionRows
            onFixRequested: function(stableId, actionKey, sourceKindKey) {
                root.fixOverviewSource(stableId, actionKey, sourceKindKey);
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

        Monitor.CostSummaryCard {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            providers: overview.providers
            subscriptionTools: overview.tools
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: overviewState.reportingProviders.length > 0

            PlasmaExtras.Heading {
                level: 4
                text: i18n("Reporting providers")
                Layout.fillWidth: true
            }
            PlasmaComponents.Label {
                text: overviewState.reportingProviders.length
                opacity: 0.7
            }
        }

        Repeater {
            model: overviewState.reportingProviders

            Monitor.ProviderCard {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                providerName: modelData.name
                providerIcon: modelData.iconSource || modelData.backend?.iconName || "globe"
                providerColor: modelData.color
                backend: modelData.backend || null
                readiness: overviewState.rowForProvider(modelData)
                scheduler: root.refreshScheduler
                showCost: true
                showUsage: true
                collapsed: false
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: overviewState.connectivityProviders.length > 0

            PlasmaExtras.Heading {
                level: 4
                text: i18n("Connection checks")
                Layout.fillWidth: true
            }
            PlasmaComponents.Button {
                text: overview.connectionChecksExpanded ? i18n("Hide")
                                                        : i18np("Show %1 source", "Show %1 sources",
                                                                 overviewState.connectivityProviders.length)
                icon.name: overview.connectionChecksExpanded ? "arrow-up" : "arrow-down"
                checkable: true
                checked: overview.connectionChecksExpanded
                activeFocusOnTab: true
                Accessible.name: overview.connectionChecksExpanded
                    ? i18n("Hide connectivity-only providers")
                    : i18n("Show connectivity-only providers")
                onClicked: overview.connectionChecksExpanded = !overview.connectionChecksExpanded
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: overviewState.connectivityProviders.length > 0 && !overview.connectionChecksExpanded
            text: i18n("These sources confirm access to an endpoint but do not report token usage or spend.")
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: overview.connectionChecksExpanded ? overviewState.connectivityProviders : []

            Monitor.ProviderCard {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                providerName: modelData.name
                providerIcon: modelData.iconSource || modelData.backend?.iconName || "globe"
                providerColor: modelData.color
                backend: modelData.backend || null
                readiness: overviewState.rowForProvider(modelData)
                scheduler: root.refreshScheduler
                showCost: false
                showUsage: false
                collapsed: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            visible: root.enabledToolCount > 0

            PlasmaExtras.Heading {
                level: 4
                text: i18n("Subscription tools")
                Layout.fillWidth: true
            }
            PlasmaComponents.ToolButton {
                visible: plasmoid.configuration.browserSyncEnabled
                icon.name: "view-refresh"
                activeFocusOnTab: true
                Accessible.name: i18n("Request Browser Sync")
                onClicked: root.performBrowserSync()
                PlasmaComponents.ToolTip { text: i18n("Request Browser Sync") }
            }
        }

        Repeater {
            model: overview.tools

            Monitor.SubscriptionToolCard {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                visible: modelData.enabled
                toolName: modelData.name
                toolIcon: modelData.iconSource || modelData.monitor?.iconName || "utilities-terminal"
                toolColor: modelData.monitor?.toolColor || Kirigami.Theme.textColor
                monitor: modelData.monitor || null
                collapsed: false
                onSyncRequested: root.performBrowserSync()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.smallSpacing
        }
    }
}
