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

    ColumnLayout {
        width: overview.availableWidth
        spacing: Kirigami.Units.mediumSpacing

        Components.StatusHeader {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            providers: overview.providers
            tools: overview.tools
        }

        Components.AttentionList {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            providers: overview.providers
            tools: overview.tools
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

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: root.pluginVersionMismatch
            text: i18n("Frontend/plugin version mismatch. Install the matching COPR package or rebuild from this exact source tag.")
            color: Kirigami.Theme.negativeTextColor
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
            PlasmaExtras.Heading { level: 4; text: i18n("Providers"); Layout.fillWidth: true }
            PlasmaComponents.Label {
                text: i18n("%1 enabled", overview.providers.filter(function(p) { return p.enabled; }).length)
                opacity: 0.7
            }
        }

        Repeater {
            model: overview.providers
            Monitor.ProviderCard {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                visible: modelData.enabled
                providerName: modelData.name
                providerIcon: modelData.iconSource || modelData.backend?.iconName || "globe"
                providerColor: modelData.color
                backend: modelData.backend || null
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
            visible: root.enabledToolCount > 0
            PlasmaExtras.Heading { level: 4; text: i18n("Subscription tools"); Layout.fillWidth: true }
            PlasmaComponents.ToolButton {
                visible: plasmoid.configuration.browserSyncEnabled
                icon.name: "view-refresh"
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

        Item { Layout.fillWidth: true; Layout.preferredHeight: Kirigami.Units.smallSpacing }
    }
}
