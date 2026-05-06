import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0

PlasmaExtras.Representation {
    id: fullRoot

    implicitWidth: Kirigami.Units.gridUnit * 28
    implicitHeight: Kirigami.Units.gridUnit * 28

    property var detailSnapshots: []
    property var detailSummaryData: ({})
    property var detailDailyCosts: []
    property string detailProviderLabel: ""

    property var compareSeriesData: []
    property date lastQueryFrom: new Date(0)
    property date lastQueryTo: new Date(0)
    property bool historyLoading: false
    property int onboardingStep: 0
    property string lastExpandedProviderName: ""
    property string lastExpandedToolName: ""
    readonly property bool narrowPopup: width < Kirigami.Units.gridUnit * 18
    readonly property bool compactHistoryControls: width < Kirigami.Units.gridUnit * 15
    readonly property bool compactSectionHeaders: width < Kirigami.Units.gridUnit * 16
    readonly property bool compactCompareRanking: width < Kirigami.Units.gridUnit * 17
    readonly property int enabledProviderCount: {
        var providers = root.allProviders ?? [];
        var count = 0;
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].enabled) count++;
        }
        return count;
    }
    readonly property int connectedProviderCount: root.connectedCount ?? 0
    readonly property bool hasAnyConnectedProvider: connectedProviderCount > 0

    readonly property bool compareMode: historyModeCombo.currentValue === "compare"
    readonly property bool detailHistoryHasSelection: selectedDetailProviderDbName() !== ""
    readonly property bool detailHistoryReady: detailHistoryState() === "ready"
    readonly property bool compareHistoryReady: compareHistoryState() === "ready"
    readonly property string dashboardMode: plasmoid.configuration.dashboardMode || "operator"
    readonly property bool showOnlyProblems: plasmoid.configuration.showOnlyProblems || false
    readonly property bool onboardingVisible: !hasAnyProvider()
        && !plasmoid.configuration.setupWizardCompleted
        && !plasmoid.configuration.setupWizardDismissed

    header: PlasmaExtras.PlasmoidHeading {
        RowLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: Qt.resolvedUrl("../icons/logo.png")
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
            }

            PlasmaExtras.Heading {
                level: 3
                text: i18n("AI Usage Monitor")
                Layout.fillWidth: true
            }

            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "view-refresh"
                onClicked: root.refreshAll()
                PlasmaComponents.ToolTip { text: i18n("Refresh all providers") }
            }

            PlasmaComponents.ToolButton {
                activeFocusOnTab: true
                icon.name: "configure"
                onClicked: plasmoid.internalAction("configure").trigger()
                PlasmaComponents.ToolTip { text: i18n("Configure...") }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !hasAnyProvider()

            PlasmaExtras.PlaceholderMessage {
                anchors.fill: parent
                visible: !fullRoot.onboardingVisible
                iconName: "preferences-desktop-notification"
                text: i18n("Welcome to AI Usage Monitor")
                explanation: i18n("Start with a provider, local tool tracking, diagnostics, demo mode, or a preset.")

                helpfulAction: QQC2.Action {
                    icon.name: "configure"
                    text: i18n("Add Provider")
                    onTriggered: plasmoid.internalAction("configure").trigger()
                }
            }

            RowLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Kirigami.Units.largeSpacing
                visible: !fullRoot.onboardingVisible
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.ToolButton {
                    icon.name: "tools-report-bug"
                    text: i18n("Diagnostics")
                    onClicked: plasmoid.internalAction("configure").trigger()
                    PlasmaComponents.ToolTip { text: i18n("Run diagnostics") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "utilities-terminal"
                    text: i18n("Local Tools")
                    onClicked: plasmoid.internalAction("configure").trigger()
                    PlasmaComponents.ToolTip { text: i18n("Enable local tool tracking") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "media-playback-start"
                    text: i18n("Demo")
                    onClicked: Qt.openUrlExternally(Qt.resolvedUrl("../../../docs/demo/fedora-kde-vm.md"))
                    PlasmaComponents.ToolTip { text: i18n("Use demo mode") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "document-import"
                    text: i18n("Preset")
                    onClicked: plasmoid.internalAction("configure").trigger()
                    PlasmaComponents.ToolTip { text: i18n("Import preset") }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 20)
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.backgroundColor
                border.width: 1
                border.color: Kirigami.Theme.disabledTextColor
                visible: fullRoot.onboardingVisible

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.mediumSpacing

                    PlasmaExtras.Heading {
                        level: 4
                        text: i18n("Set Up AI Usage Monitor")
                        Layout.fillWidth: true
                    }

                    PlasmaComponents.Label {
                        text: i18n("Step %1 of 4", fullRoot.onboardingStep + 1)
                        opacity: 0.7
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: {
                            if (fullRoot.onboardingStep === 0) {
                                return i18n("Choose at least one provider in Configure > Providers. OpenAI, Anthropic, Google, and OpenAI-compatible providers are supported.");
                            }
                            if (fullRoot.onboardingStep === 1) {
                                return i18n("Add your API key for that provider. Keys are stored in KWallet and are never saved in plain text config files.");
                            }
                            if (fullRoot.onboardingStep === 2) {
                                return i18n("Optional: configure subscription tools (Claude Code, Codex CLI, GitHub Copilot) in Configure > Subscriptions to track fixed plan usage and monthly cost.");
                            }
                            return i18n("Return to this widget and use Refresh All. Once one provider is enabled, the live dashboard appears automatically.");
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Button {
                            activeFocusOnTab: true
                            text: i18n("Open Provider Settings")
                            icon.name: "configure"
                            onClicked: plasmoid.internalAction("configure").trigger()
                        }

                        Item { Layout.fillWidth: true }

                        PlasmaComponents.Button {
                            activeFocusOnTab: true
                            text: i18n("Not now")
                            onClicked: {
                                plasmoid.configuration.setupWizardDismissed = true;
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Button {
                            activeFocusOnTab: true
                            text: i18n("Back")
                            enabled: fullRoot.onboardingStep > 0
                            onClicked: {
                                fullRoot.onboardingStep = Math.max(0, fullRoot.onboardingStep - 1);
                            }
                        }

                        Item { Layout.fillWidth: true }

                        PlasmaComponents.Button {
                            activeFocusOnTab: true
                            visible: fullRoot.onboardingStep < 3
                            text: i18n("Next")
                            onClicked: {
                                fullRoot.onboardingStep = Math.min(3, fullRoot.onboardingStep + 1);
                            }
                        }

                        PlasmaComponents.Button {
                            activeFocusOnTab: true
                            visible: fullRoot.onboardingStep === 3
                            text: i18n("Done")
                            onClicked: {
                                plasmoid.configuration.setupWizardCompleted = true;
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            visible: hasAnyProvider()
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: root.allProviders ?? []

                Rectangle {
                    visible: modelData.enabled
                    width: Kirigami.Units.smallSpacing * 3
                    height: width
                    radius: width / 2
                    color: {
                        if (!modelData.backend) return Kirigami.Theme.disabledTextColor;
                        if (modelData.backend.error) return Kirigami.Theme.negativeTextColor;
                        if (modelData.backend.connected) return Kirigami.Theme.positiveTextColor;
                        if (modelData.backend.loading) return Kirigami.Theme.neutralTextColor;
                        return Kirigami.Theme.disabledTextColor;
                    }

                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }

                    PlasmaComponents.ToolTip {
                        text: modelData.name + ": " + (modelData.backend?.connected ? i18n("Connected") : i18n("Disconnected"))
                    }
                }
            }

            Item { Layout.fillWidth: true }

            PlasmaComponents.Label {
                text: i18n("%1 active", root.connectedCount ?? 0)
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.6
            }
        }

        QQC2.TabBar {
            id: tabBar
            Layout.fillWidth: true
            visible: hasAnyProvider()

            QQC2.TabButton {
                activeFocusOnTab: true
                text: i18n("Live")
                width: implicitWidth
            }
            QQC2.TabButton {
                activeFocusOnTab: true
                text: i18n("History")
                width: implicitWidth
            }
            QQC2.TabButton {
                activeFocusOnTab: true
                text: i18n("Analyst")
                width: implicitWidth
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: hasAnyProvider()
            currentIndex: tabBar.currentIndex

            PlasmaComponents.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                contentItem: Flickable {
                    flickableDirection: Flickable.VerticalFlick
                    contentWidth: width
                    contentHeight: liveColumn.implicitHeight
                    interactive: contentHeight > height
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    ColumnLayout {
                        id: liveColumn
                        width: parent.width
                        spacing: Kirigami.Units.mediumSpacing

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            radius: Kirigami.Units.cornerRadius
                            color: Kirigami.Theme.backgroundColor
                            border.width: 1
                            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.1)
                            visible: hasAnyProvider()
                            implicitHeight: summaryLayout.implicitHeight + Kirigami.Units.smallSpacing * 2

                            GridLayout {
                                id: summaryLayout
                                anchors.fill: parent
                                anchors.margins: Kirigami.Units.smallSpacing
                                columns: fullRoot.narrowPopup ? 2 : 4
                                columnSpacing: Kirigami.Units.largeSpacing
                                rowSpacing: Kirigami.Units.smallSpacing

                                ColumnLayout {
                                    spacing: 0
                                    PlasmaComponents.Label {
                                        text: i18n("Providers")
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        opacity: 0.7
                                    }
                                    PlasmaComponents.Label {
                                        text: i18n("%1 enabled", fullRoot.enabledProviderCount)
                                        font.bold: true
                                    }
                                }

                                ColumnLayout {
                                    spacing: 0
                                    PlasmaComponents.Label {
                                        text: i18n("Connected")
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        opacity: 0.7
                                    }
                                    PlasmaComponents.Label {
                                        text: i18n("%1 active", fullRoot.connectedProviderCount)
                                        font.bold: true
                                        color: fullRoot.hasAnyConnectedProvider
                                            ? Kirigami.Theme.positiveTextColor
                                            : Kirigami.Theme.disabledTextColor
                                    }
                                }

                                ColumnLayout {
                                    spacing: 0
                                    PlasmaComponents.Label {
                                        text: i18n("Total Cost")
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        opacity: 0.7
                                    }
                                    PlasmaComponents.Label {
                                        text: "$" + (root.totalCost ?? 0).toFixed(2)
                                        font.bold: true
                                    }
                                }

                                ColumnLayout {
                                    spacing: 0
                                    PlasmaComponents.Label {
                                        text: i18n("Tool Monitors")
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        opacity: 0.7
                                    }
                                    PlasmaComponents.Label {
                                        text: i18n("%1 enabled", root.enabledToolCount ?? 0)
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.ComboBox {
                                id: dashboardModeCombo
                                activeFocusOnTab: true
                                textRole: "text"
                                valueRole: "value"
                                model: [
                                    { text: i18n("Simple"), value: "simple" },
                                    { text: i18n("Operator"), value: "operator" },
                                    { text: i18n("Analyst"), value: "analyst" }
                                ]
                                Component.onCompleted: {
                                    var mode = fullRoot.dashboardMode;
                                    for (var i = 0; i < model.length; i++) {
                                        if (model[i].value === mode) {
                                            currentIndex = i;
                                            return;
                                        }
                                    }
                                    currentIndex = 1;
                                }
                                onActivated: {
                                    plasmoid.configuration.dashboardMode = currentValue;
                                    tabBar.currentIndex = currentValue === "analyst" ? 2 : 0;
                                }
                            }

                            QQC2.CheckBox {
                                activeFocusOnTab: true
                                text: i18n("Only Problems")
                                checked: fullRoot.showOnlyProblems
                                onToggled: plasmoid.configuration.showOnlyProblems = checked
                            }

                            Item { Layout.fillWidth: true }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            visible: attentionItemCount() > 0
                            spacing: Kirigami.Units.smallSpacing

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                PlasmaExtras.Heading {
                                    level: 5
                                    text: i18n("Needs Attention")
                                    Layout.fillWidth: true
                                    opacity: 0.82
                                }

                                Rectangle {
                                    radius: Kirigami.Units.smallSpacing
                                    color: Qt.alpha(Kirigami.Theme.negativeTextColor, 0.12)
                                    border.width: 1
                                    border.color: Qt.alpha(Kirigami.Theme.negativeTextColor, 0.24)
                                    implicitWidth: attentionCountLabel.implicitWidth + Kirigami.Units.smallSpacing * 2
                                    implicitHeight: attentionCountLabel.implicitHeight + Kirigami.Units.smallSpacing

                                    PlasmaComponents.Label {
                                        id: attentionCountLabel
                                        anchors.centerIn: parent
                                        text: i18n("%1 items", attentionItemCount())
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        color: Kirigami.Theme.negativeTextColor
                                    }
                                }
                            }

                            Repeater {
                                model: root.allProviders ?? []

                                Rectangle {
                                    readonly property string reason: fullRoot.providerAttentionReason(modelData)
                                    Layout.fillWidth: true
                                    visible: modelData.enabled && reason !== ""
                                    radius: Kirigami.Units.smallSpacing
                                    color: Qt.alpha(Kirigami.Theme.negativeTextColor, 0.06)
                                    border.width: 1
                                    border.color: Qt.alpha(Kirigami.Theme.negativeTextColor, 0.18)
                                    implicitHeight: providerAttentionRow.implicitHeight + Kirigami.Units.smallSpacing * 2

                                    RowLayout {
                                        id: providerAttentionRow
                                        anchors.fill: parent
                                        anchors.margins: Kirigami.Units.smallSpacing
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Icon {
                                            source: modelData.iconSource || modelData.backend?.iconName || "dialog-warning"
                                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            PlasmaComponents.Label {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            PlasmaComponents.Label {
                                                Layout.fillWidth: true
                                                text: reason
                                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                                opacity: 0.72
                                                wrapMode: Text.WordWrap
                                            }
                                        }

                                        PlasmaComponents.Button {
                                            activeFocusOnTab: true
                                            text: i18n("Review")
                                            onClicked: fullRoot.lastExpandedProviderName = modelData.name
                                        }
                                    }
                                }
                            }

                            Repeater {
                                model: root.allSubscriptionTools ?? []

                                Rectangle {
                                    readonly property string reason: fullRoot.toolAttentionReason(modelData)
                                    Layout.fillWidth: true
                                    visible: modelData.enabled && reason !== ""
                                    radius: Kirigami.Units.smallSpacing
                                    color: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.08)
                                    border.width: 1
                                    border.color: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.2)
                                    implicitHeight: toolAttentionRow.implicitHeight + Kirigami.Units.smallSpacing * 2

                                    RowLayout {
                                        id: toolAttentionRow
                                        anchors.fill: parent
                                        anchors.margins: Kirigami.Units.smallSpacing
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Icon {
                                            source: modelData.iconSource || modelData.monitor?.iconName || "dialog-warning"
                                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            PlasmaComponents.Label {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            PlasmaComponents.Label {
                                                Layout.fillWidth: true
                                                text: reason
                                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                                opacity: 0.72
                                                wrapMode: Text.WordWrap
                                            }
                                        }

                                        PlasmaComponents.Button {
                                            activeFocusOnTab: true
                                            text: i18n("Review")
                                            onClicked: fullRoot.lastExpandedToolName = modelData.name
                                        }
                                    }
                                }
                            }
                        }

                        CostSummaryCard {
                            Layout.fillWidth: true
                            Layout.margins: Kirigami.Units.smallSpacing
                            visible: hasCostData() && fullRoot.dashboardMode !== "simple"
                            providers: root.allProviders ?? []
                            subscriptionTools: root.allSubscriptionTools ?? []
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            visible: fullRoot.enabledProviderCount > 0
                            spacing: fullRoot.compactSectionHeaders ? 2 : Kirigami.Units.smallSpacing

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                PlasmaExtras.Heading {
                                    level: 5
                                    text: i18n("Providers")
                                    Layout.fillWidth: true
                                    opacity: 0.78
                                }

                                Rectangle {
                                    radius: Kirigami.Units.smallSpacing
                                    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.12)
                                    border.width: 1
                                    border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.26)
                                    implicitWidth: providerStatusLabel.implicitWidth + Kirigami.Units.smallSpacing * 2
                                    implicitHeight: providerStatusLabel.implicitHeight + Kirigami.Units.smallSpacing

                                    PlasmaComponents.Label {
                                        id: providerStatusLabel
                                        anchors.centerIn: parent
                                        text: i18n("%1/%2 connected", fullRoot.connectedProviderCount, fullRoot.enabledProviderCount)
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        color: fullRoot.hasAnyConnectedProvider
                                            ? Kirigami.Theme.textColor
                                            : Kirigami.Theme.disabledTextColor
                                    }
                                }
                            }
                        }

                        PlasmaExtras.PlaceholderMessage {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            visible: fullRoot.enabledProviderCount > 0
                                     && !fullRoot.hasAnyConnectedProvider
                                     && attentionItemCount() === 0
                            iconName: "network-disconnect"
                            text: i18n("Providers are enabled but not connected")
                            explanation: i18n("Verify API keys, endpoint URLs, and connectivity, then use Refresh All.")
                        }

                        Repeater {
                            model: root.allProviders ?? []

                            ProviderCard {
                                Layout.fillWidth: true
                                visible: fullRoot.providerVisibleInLive(modelData)
                                providerName: modelData.name
                                providerIcon: modelData.iconSource || modelData.backend?.iconName || "globe"
                                providerColor: modelData.color
                                backend: modelData.backend ?? null
                                showCost: true
                                showUsage: true
                                collapsed: fullRoot.shouldCollapseProviderCard(modelData)

                                onCollapsedChanged: {
                                    if (!collapsed) {
                                        fullRoot.lastExpandedProviderName = modelData.name;
                                    } else if (fullRoot.lastExpandedProviderName === modelData.name) {
                                        fullRoot.lastExpandedProviderName = "";
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            Layout.topMargin: Kirigami.Units.largeSpacing
                            visible: root.enabledToolCount > 0
                            spacing: fullRoot.compactSectionHeaders ? 2 : Kirigami.Units.smallSpacing

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                PlasmaExtras.Heading {
                                    level: 5
                                    text: i18n("Subscription Tools")
                                    Layout.fillWidth: true
                                    opacity: 0.7
                                }

                                PlasmaComponents.ToolButton {
                                    activeFocusOnTab: true
                                    icon.name: "view-refresh"
                                    display: PlasmaComponents.AbstractButton.IconOnly
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                    visible: plasmoid.configuration.browserSyncEnabled
                                    onClicked: root.performBrowserSync()
                                    PlasmaComponents.ToolTip { text: i18n("Sync usage data from browser") }
                                }
                            }
                        }

                        Repeater {
                            model: root.allSubscriptionTools ?? []

                            SubscriptionToolCard {
                                Layout.fillWidth: true
                                Layout.leftMargin: Kirigami.Units.smallSpacing
                                Layout.rightMargin: Kirigami.Units.smallSpacing
                                visible: fullRoot.toolVisibleInLive(modelData)
                                toolName: modelData.name
                                toolIcon: modelData.iconSource || modelData.monitor?.iconName || "utilities-terminal"
                                toolColor: modelData.monitor?.toolColor ?? Kirigami.Theme.textColor
                                monitor: modelData.monitor ?? null
                                collapsed: fullRoot.shouldCollapseToolCard(modelData)

                                onSyncRequested: {
                                    root.performBrowserSync();
                                }

                                onCollapsedChanged: {
                                    if (!collapsed) {
                                        fullRoot.lastExpandedToolName = modelData.name;
                                    } else if (fullRoot.lastExpandedToolName === modelData.name) {
                                        fullRoot.lastExpandedToolName = "";
                                    }
                                }
                            }
                        }

                        PlasmaExtras.PlaceholderMessage {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
                            visible: fullRoot.showOnlyProblems && attentionItemCount() === 0
                            iconName: "dialog-ok"
                            text: i18n("No Problems")
                            explanation: i18n("All enabled providers and local tools are currently clear.")
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Kirigami.Units.smallSpacing

                Flickable {
                    id: historyControlsFlick
                    Layout.fillWidth: true
                    Layout.preferredHeight: historyControlsRow.implicitHeight + Kirigami.Units.smallSpacing * 2
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    clip: true
                    contentWidth: historyControlsRow.implicitWidth + Kirigami.Units.smallSpacing * 2
                    contentHeight: height
                    interactive: contentWidth > width
                    boundsBehavior: Flickable.StopAtBounds

                    RowLayout {
                        id: historyControlsRow
                        x: Kirigami.Units.smallSpacing
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            text: i18n("View:")
                            visible: !fullRoot.compactHistoryControls
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }

                        QQC2.ComboBox {
                            activeFocusOnTab: true
                            id: historyModeCombo
                            model: [
                                { text: i18n("Detail"), value: "detail" },
                                { text: i18n("Compare"), value: "compare" }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: 0
                            Layout.preferredWidth: fullRoot.compactHistoryControls ? Kirigami.Units.gridUnit * 5.5
                                : (fullRoot.narrowPopup ? Kirigami.Units.gridUnit * 6 : implicitWidth)
                            onCurrentIndexChanged: {
                                refreshHistory();
                            }
                            PlasmaComponents.ToolTip { text: i18n("History view mode") }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Source:")
                            visible: fullRoot.compareMode && !fullRoot.compactHistoryControls
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }

                        QQC2.ComboBox {
                            activeFocusOnTab: true
                            id: compareSourceCombo
                            visible: fullRoot.compareMode
                            model: [
                                { text: i18n("Providers"), value: "providers" },
                                { text: i18n("Subscription Tools"), value: "tools" }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: 0
                            Layout.preferredWidth: fullRoot.compactHistoryControls ? Kirigami.Units.gridUnit * 7
                                : (fullRoot.narrowPopup ? Kirigami.Units.gridUnit * 8 : implicitWidth)
                            onCurrentIndexChanged: {
                                resetCompareMetric();
                                refreshHistory();
                            }
                            PlasmaComponents.ToolTip { text: i18n("Comparison source") }
                        }

                        QQC2.ComboBox {
                            activeFocusOnTab: true
                            id: historyProviderCombo
                            visible: !fullRoot.compareMode
                            model: getEnabledProviderEntries()
                            textRole: "label"
                            valueRole: "dbName"
                            Layout.preferredWidth: fullRoot.compactHistoryControls ? Kirigami.Units.gridUnit * 7.5
                                : (fullRoot.narrowPopup ? Kirigami.Units.gridUnit * 9 : Kirigami.Units.gridUnit * 11)
                            onCurrentIndexChanged: {
                                if (!fullRoot.compareMode) refreshHistory();
                            }
                            PlasmaComponents.ToolTip { text: i18n("History provider") }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Metric:")
                            visible: fullRoot.compareMode && !fullRoot.compactHistoryControls
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }

                        QQC2.ComboBox {
                            activeFocusOnTab: true
                            id: compareMetricCombo
                            visible: fullRoot.compareMode
                            model: getCompareMetricOptions(compareSourceCombo.currentValue)
                            textRole: "text"
                            valueRole: "value"
                            Layout.preferredWidth: fullRoot.compactHistoryControls ? Kirigami.Units.gridUnit * 7
                                : (fullRoot.narrowPopup ? Kirigami.Units.gridUnit * 8 : implicitWidth)
                            onCurrentIndexChanged: {
                                if (fullRoot.compareMode) refreshHistory();
                            }
                            PlasmaComponents.ToolTip { text: i18n("Comparison metric") }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Range:")
                            visible: !fullRoot.compactHistoryControls
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }

                        QQC2.ComboBox {
                            activeFocusOnTab: true
                            id: timeRangeCombo
                            model: [i18n("24 hours"), i18n("7 days"), i18n("30 days")]
                            currentIndex: 1
                            Layout.preferredWidth: fullRoot.compactHistoryControls ? Kirigami.Units.gridUnit * 5.5
                                : (fullRoot.narrowPopup ? Kirigami.Units.gridUnit * 6 : implicitWidth)
                            onCurrentIndexChanged: refreshHistory()
                            PlasmaComponents.ToolTip { text: i18n("Time range") }
                        }

                        PlasmaComponents.ToolButton {
                            activeFocusOnTab: true
                            icon.name: "view-refresh"
                            enabled: canRefreshHistory() && !fullRoot.historyLoading
                            onClicked: refreshHistory()
                            PlasmaComponents.ToolTip { text: i18n("Refresh history") }
                        }
                    }

                    QQC2.ScrollBar.horizontal: QQC2.ScrollBar {
                        policy: historyControlsFlick.contentWidth > historyControlsFlick.width
                            ? QQC2.ScrollBar.AsNeeded
                            : QQC2.ScrollBar.AlwaysOff
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Kirigami.Units.smallSpacing
                        visible: !fullRoot.compareMode && fullRoot.detailHistoryHasSelection

                        UsageChart {
                            id: historyChart
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 10
                            Layout.margins: Kirigami.Units.smallSpacing
                            showMetricBar: fullRoot.detailHistoryHasSelection
                            showChartContent: fullRoot.detailHistoryReady
                            showEmptyState: false
                            chartData: fullRoot.detailSnapshots
                        }

                        TrendSummary {
                            id: trendSummary
                            Layout.fillWidth: true
                            Layout.margins: Kirigami.Units.smallSpacing
                            visible: fullRoot.detailHistoryReady
                            showEmptyState: false
                            summaryData: fullRoot.detailSummaryData
                            dailyCosts: fullRoot.detailDailyCosts
                            provider: fullRoot.detailProviderLabel
                        }
                    }

                    PlasmaExtras.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.largeSpacing * 2
                        visible: detailHistoryState() === "select-provider"
                        iconName: "office-chart-line"
                        text: i18n("Select a provider")
                        explanation: i18n("Choose a provider to view historical trends")
                    }

                    PlasmaExtras.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.largeSpacing * 2
                        visible: detailHistoryState() === "no-data"
                        iconName: "view-calendar-timeline"
                        text: i18n("No historical data")
                        explanation: i18n("No snapshots were found for this provider and time range")
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Kirigami.Units.smallSpacing
                        visible: fullRoot.compareHistoryReady

                        MultiSeriesChart {
                            id: compareChart
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                            Layout.margins: Kirigami.Units.smallSpacing
                            showEmptyState: false
                            metric: currentCompareMetric()
                            seriesData: fullRoot.compareSeriesData
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.margins: Kirigami.Units.smallSpacing
                            Layout.preferredHeight: rankingColumn.implicitHeight + Kirigami.Units.smallSpacing * 2
                            radius: Kirigami.Units.cornerRadius
                            color: Qt.alpha(Kirigami.Theme.highlightColor, 0.06)
                            border.width: 1
                            border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.2)
                            visible: hasCompareData()

                            ColumnLayout {
                                id: rankingColumn
                                anchors {
                                    fill: parent
                                    margins: Kirigami.Units.smallSpacing
                                }
                                spacing: Kirigami.Units.smallSpacing

                                PlasmaComponents.Label {
                                    text: i18n("Top Contributors")
                                    font.bold: true
                                }

                                Repeater {
                                    model: compareRanking()

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: fullRoot.compactCompareRanking ? 2 : 0

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            PlasmaComponents.Label {
                                                text: (index + 1) + "."
                                                opacity: 0.6
                                            }

                                            Rectangle {
                                                width: 8
                                                height: 8
                                                radius: 4
                                                color: modelData.color
                                            }

                                            PlasmaComponents.Label {
                                                text: modelData.name
                                                Layout.fillWidth: true
                                                elide: fullRoot.compactCompareRanking ? Text.ElideNone : Text.ElideRight
                                                wrapMode: fullRoot.compactCompareRanking ? Text.WordWrap : Text.NoWrap
                                                maximumLineCount: fullRoot.compactCompareRanking ? 2 : 1
                                            }

                                            PlasmaComponents.Label {
                                                visible: !fullRoot.compactCompareRanking
                                                text: formatCompareValue(modelData.latestValue)
                                                font.bold: true
                                            }

                                            PlasmaComponents.Label {
                                                visible: !fullRoot.compactCompareRanking
                                                text: modelData.deltaPercent > 0
                                                    ? i18n("↑ %1%", Math.abs(modelData.deltaPercent).toFixed(1))
                                                    : (modelData.deltaPercent < 0
                                                       ? i18n("↓ %1%", Math.abs(modelData.deltaPercent).toFixed(1))
                                                       : i18n("→ 0%"))
                                                color: modelData.deltaPercent > 0
                                                    ? Kirigami.Theme.negativeTextColor
                                                    : (modelData.deltaPercent < 0
                                                       ? Kirigami.Theme.positiveTextColor
                                                       : Kirigami.Theme.disabledTextColor)
                                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: fullRoot.compactCompareRanking
                                            spacing: Kirigami.Units.smallSpacing

                                            Item { Layout.fillWidth: true }

                                            PlasmaComponents.Label {
                                                text: formatCompareValue(modelData.latestValue)
                                                font.bold: true
                                            }

                                            PlasmaComponents.Label {
                                                text: modelData.deltaPercent > 0
                                                    ? i18n("↑ %1%", Math.abs(modelData.deltaPercent).toFixed(1))
                                                    : (modelData.deltaPercent < 0
                                                       ? i18n("↓ %1%", Math.abs(modelData.deltaPercent).toFixed(1))
                                                       : i18n("→ 0%"))
                                                color: modelData.deltaPercent > 0
                                                    ? Kirigami.Theme.negativeTextColor
                                                    : (modelData.deltaPercent < 0
                                                       ? Kirigami.Theme.positiveTextColor
                                                       : Kirigami.Theme.disabledTextColor)
                                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    PlasmaExtras.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.largeSpacing * 2
                        visible: compareHistoryState() === "no-data"
                        iconName: "office-chart-line"
                        text: i18n("No comparison data")
                        explanation: i18n("No data was found for the selected source, metric, and range")
                    }

                    PlasmaExtras.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.largeSpacing * 2
                        visible: compareHistoryState() === "no-sources"
                        iconName: "dialog-information"
                        text: i18n("No sources available")
                        explanation: i18n("Enable at least one provider or subscription tool to compare trends")
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: fullRoot.historyLoading
                        color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.82)
                        z: 30

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.BusyIndicator {
                                running: fullRoot.historyLoading
                                visible: fullRoot.historyLoading
                                Layout.alignment: Qt.AlignHCenter
                            }

                            PlasmaComponents.Label {
                                text: i18n("Loading history...")
                                opacity: 0.75
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Item { Layout.fillWidth: true }

                    PlasmaComponents.ToolButton {
                        activeFocusOnTab: true
                        icon.name: "document-export"
                        display: fullRoot.compactHistoryControls
                            ? PlasmaComponents.AbstractButton.IconOnly
                            : PlasmaComponents.AbstractButton.TextBesideIcon
                        text: i18n("CSV")
                        enabled: canExportCurrentView()
                        onClicked: exportData("csv")
                        PlasmaComponents.ToolTip { text: i18n("Export current view as CSV") }
                    }

                    PlasmaComponents.ToolButton {
                        activeFocusOnTab: true
                        icon.name: "document-export"
                        display: fullRoot.compactHistoryControls
                            ? PlasmaComponents.AbstractButton.IconOnly
                            : PlasmaComponents.AbstractButton.TextBesideIcon
                        text: i18n("JSON")
                        enabled: canExportCurrentView()
                        onClicked: exportData("json")
                        PlasmaComponents.ToolTip { text: i18n("Export current view as JSON") }
                    }
                }
            }

            AnalystTab {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            visible: hasAnyProvider()
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.6
            text: {
                var interval = plasmoid.configuration.refreshInterval;
                var dbSize = root.usageDb ? root.usageDb.databaseSize() : 0;
                var dbText = dbSize > 0 ? i18n(" · DB %1 KB", Math.round(dbSize / 1024)) : "";
                if (interval >= 60) {
                    return i18n("Auto-refresh every %1 min", Math.floor(interval / 60)) + dbText;
                }
                return i18n("Auto-refresh every %1 sec", interval) + dbText;
            }
        }
    }

    function hasAnyProvider() {
        if (AppInfo.demoMode) {
            return true;
        }
        return plasmoid.configuration.openaiEnabled
            || plasmoid.configuration.anthropicEnabled
            || plasmoid.configuration.googleEnabled
            || plasmoid.configuration.mistralEnabled
            || plasmoid.configuration.deepseekEnabled
            || plasmoid.configuration.groqEnabled
            || plasmoid.configuration.xaiEnabled
            || plasmoid.configuration.ollamaEnabled
            || plasmoid.configuration.openrouterEnabled
            || plasmoid.configuration.togetherEnabled
            || plasmoid.configuration.cohereEnabled
            || plasmoid.configuration.googleveoEnabled
            || plasmoid.configuration.claudeCodeEnabled
            || plasmoid.configuration.codexEnabled
            || plasmoid.configuration.copilotEnabled
            || plasmoid.configuration.cursorEnabled
            || plasmoid.configuration.windsurfEnabled
            || plasmoid.configuration.jetbrainsAiEnabled;
    }

    function providerRateLimitPercent(backend) {
        if (!backend) return 0;
        var requestPercent = 0;
        var tokenPercent = 0;
        if ((backend.rateLimitRequests ?? 0) > 0) {
            requestPercent = 100 - ((backend.rateLimitRequestsRemaining ?? 0) * 100 / backend.rateLimitRequests);
        }
        if ((backend.rateLimitTokens ?? 0) > 0) {
            tokenPercent = 100 - ((backend.rateLimitTokensRemaining ?? 0) * 100 / backend.rateLimitTokens);
        }
        return Math.max(requestPercent, tokenPercent);
    }

    function providerAttentionReason(provider) {
        if (!provider || !provider.enabled || !provider.backend) return "";
        var backend = provider.backend;
        var warningPercent = plasmoid.configuration.budgetWarningPercent || 80;
        if (backend.error) return i18n("Error: %1", backend.error);
        if (!backend.connected) return i18n("Not connected");
        if ((backend.dailyBudget ?? 0) > 0 && (backend.dailyCost ?? 0) >= (backend.dailyBudget * warningPercent / 100.0)) {
            return i18n("Daily budget nearing limit");
        }
        if ((backend.monthlyBudget ?? 0) > 0 && (backend.monthlyCost ?? 0) >= (backend.monthlyBudget * warningPercent / 100.0)) {
            return i18n("Monthly budget nearing limit");
        }
        var rateLimitUsed = providerRateLimitPercent(backend);
        if (rateLimitUsed >= (plasmoid.configuration.warningThreshold || 80)) {
            return i18n("Rate limit usage at %1%", Math.round(rateLimitUsed));
        }
        return "";
    }

    function toolAttentionReason(tool) {
        if (!tool || !tool.enabled || !tool.monitor) return "";
        var monitor = tool.monitor;
        if (!(monitor.installed ?? false)) return i18n("Tool not detected on this system");
        if (monitor.limitReached ?? false) return i18n("Primary limit reached");
        if (monitor.secondaryLimitReached ?? false) return i18n("Secondary limit reached");
        if ((monitor.percentUsed ?? 0) >= 80) return i18n("Primary usage at %1%", Math.round(monitor.percentUsed));
        if ((monitor.secondaryPercentUsed ?? 0) >= 80) return i18n("Secondary usage at %1%", Math.round(monitor.secondaryPercentUsed));
        var syncStatus = monitor.syncStatus ?? "";
        if (syncStatus !== "" && syncStatus !== "idle" && syncStatus !== "OK") {
            return syncStatus;
        }
        return "";
    }

    function attentionItemCount() {
        var count = 0;
        var providers = root.allProviders ?? [];
        for (var i = 0; i < providers.length; i++) {
            if (providerAttentionReason(providers[i]) !== "") count++;
        }
        var tools = root.allSubscriptionTools ?? [];
        for (var j = 0; j < tools.length; j++) {
            if (toolAttentionReason(tools[j]) !== "") count++;
        }
        return count;
    }

    function shouldCollapseProviderCard(provider) {
        if (!provider || !provider.enabled) return true;
        if (fullRoot.dashboardMode === "simple") return providerAttentionReason(provider) === "";
        if (providerAttentionReason(provider) !== "") return false;
        return fullRoot.lastExpandedProviderName === ""
            ? true
            : fullRoot.lastExpandedProviderName !== provider.name;
    }

    function shouldCollapseToolCard(tool) {
        if (!tool || !tool.enabled) return true;
        if (fullRoot.dashboardMode === "simple") return toolAttentionReason(tool) === "";
        if (toolAttentionReason(tool) !== "") return false;
        return fullRoot.lastExpandedToolName === ""
            ? true
            : fullRoot.lastExpandedToolName !== tool.name;
    }

    function hasCostData() {
        var providers = root.allProviders ?? [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].enabled && providers[i].backend && providers[i].backend.cost > 0)
                return true;
        }
        var tools = root.allSubscriptionTools ?? [];
        for (var j = 0; j < tools.length; j++) {
            if (tools[j].enabled && tools[j].monitor && tools[j].monitor.hasSubscriptionCost
                && (tools[j].monitor.subscriptionCost ?? 0) > 0) {
                return true;
            }
        }
        return false;
    }

    function providerVisibleInLive(provider) {
        if (!provider || !provider.enabled) return false;
        if (!fullRoot.showOnlyProblems) return true;
        return providerAttentionReason(provider) !== "";
    }

    function toolVisibleInLive(tool) {
        if (!tool || !tool.enabled) return false;
        if (!fullRoot.showOnlyProblems) return true;
        return toolAttentionReason(tool) !== "";
    }

    function getEnabledProviderEntries() {
        var entries = [];
        var providers = root.allProviders ?? [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].enabled) {
                entries.push({ label: providers[i].name, dbName: providers[i].dbName });
            }
        }
        if (entries.length === 0) {
            entries.push({ label: i18n("No providers"), dbName: "" });
        }
        return entries;
    }

    function getEnabledProviderDbNames() {
        var names = [];
        var providers = root.allProviders ?? [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].enabled) {
                names.push(providers[i].dbName);
            }
        }
        return names;
    }

    function getEnabledToolNames() {
        var names = [];
        var tools = root.allSubscriptionTools ?? [];
        for (var i = 0; i < tools.length; i++) {
            if (tools[i].enabled) {
                names.push(tools[i].name);
            }
        }
        return names;
    }

    function selectedDetailProviderDbName() {
        return historyProviderCombo.currentValue || "";
    }

    function currentCompareMetric() {
        return compareMetricCombo.currentValue || (compareSourceCombo.currentValue === "tools" ? "percentUsed" : "cost");
    }

    function getCompareMetricOptions(source) {
        if (source === "tools") {
            return [
                { text: i18n("Percent Used"), value: "percentUsed" },
                { text: i18n("Usage Count"), value: "usageCount" },
                { text: i18n("Remaining"), value: "remaining" }
            ];
        }
        return [
            { text: i18n("Cost"), value: "cost" },
            { text: i18n("Tokens"), value: "tokens" },
            { text: i18n("Requests"), value: "requests" },
            { text: i18n("Rate Limit Used"), value: "rateLimitUsed" }
        ];
    }

    function resetCompareMetric() {
        if (compareMetricCombo.count > 0) {
            compareMetricCombo.currentIndex = 0;
        }
    }

    function getTimeRange() {
        var now = new Date();
        switch (timeRangeCombo.currentIndex) {
            case 0: return new Date(now.getTime() - 24 * 60 * 60 * 1000);
            case 1: return new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
            case 2: return new Date(now.getTime() - 30 * 24 * 60 * 60 * 1000);
            default: return new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
        }
    }

    function compareBucketMinutes() {
        switch (timeRangeCombo.currentIndex) {
            case 0: return 15;
            case 1: return 60;
            case 2: return 180;
            default: return 60;
        }
    }

    function canRefreshHistory() {
        if (fullRoot.compareMode) {
            if (compareSourceCombo.currentValue === "tools") {
                return getEnabledToolNames().length > 0;
            }
            return getEnabledProviderDbNames().length > 0;
        }
        return selectedDetailProviderDbName() !== "";
    }

    function detailHistoryState() {
        if (fullRoot.compareMode) return "hidden";
        if (fullRoot.historyLoading) return "loading";
        if (!fullRoot.detailHistoryHasSelection) return "select-provider";
        if (fullRoot.detailSnapshots.length === 0) return "no-data";
        return "ready";
    }

    function compareHistoryState() {
        if (!fullRoot.compareMode) return "hidden";
        if (fullRoot.historyLoading) return "loading";
        if (!canRefreshHistory()) return "no-sources";
        if (!hasCompareData()) return "no-data";
        return "ready";
    }

    function refreshHistory() {
        if (!root.usageDb) return;
        if (!timeRangeCombo) return;
        if (fullRoot.compareMode && (!compareSourceCombo || !compareMetricCombo)) return;
        if (!fullRoot.compareMode && !historyProviderCombo) return;
        fullRoot.historyLoading = true;
        try {
            var from = getTimeRange();
            var to = new Date();
            fullRoot.lastQueryFrom = from;
            fullRoot.lastQueryTo = to;

            if (fullRoot.compareMode) {
                var source = compareSourceCombo.currentValue || "providers";
                var metric = currentCompareMetric();
                var names = source === "tools" ? getEnabledToolNames() : getEnabledProviderDbNames();

                if (names.length === 0) {
                    fullRoot.compareSeriesData = [];
                    return;
                }

                var bucketMinutes = compareBucketMinutes();
                var rawSeries;
                if (source === "tools") {
                    rawSeries = root.usageDb.getToolSeries(names, from, to, metric, bucketMinutes);
                } else {
                    rawSeries = root.usageDb.getProviderSeries(names, from, to, metric, bucketMinutes);
                }
                fullRoot.compareSeriesData = decorateCompareSeries(rawSeries, source);
                return;
            }

            var providerDbName = selectedDetailProviderDbName();
            if (providerDbName === "") {
                fullRoot.detailSnapshots = [];
                fullRoot.detailSummaryData = ({})
                fullRoot.detailDailyCosts = [];
                return;
            }

            fullRoot.detailProviderLabel = historyProviderCombo.currentText;
            fullRoot.detailSnapshots = root.usageDb.getSnapshots(providerDbName, from, to);
            fullRoot.detailSummaryData = root.usageDb.getSummary(providerDbName, from, to);
            fullRoot.detailDailyCosts = root.usageDb.getDailyCosts(providerDbName, from, to);
        } finally {
            fullRoot.historyLoading = false;
        }
    }

    function providerDisplayName(dbName) {
        var providers = root.allProviders ?? [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].dbName === dbName) {
                return providers[i].name;
            }
        }
        return dbName;
    }

    function providerColor(dbName) {
        var providers = root.allProviders ?? [];
        for (var i = 0; i < providers.length; i++) {
            if (providers[i].dbName === dbName) {
                return providers[i].color;
            }
        }
        return Kirigami.Theme.highlightColor;
    }

    function toolColor(name) {
        var tools = root.allSubscriptionTools ?? [];
        for (var i = 0; i < tools.length; i++) {
            if (tools[i].name === name) {
                return tools[i].monitor?.toolColor || Kirigami.Theme.highlightColor;
            }
        }
        return Kirigami.Theme.highlightColor;
    }

    function decorateCompareSeries(rawSeries, source) {
        var out = [];
        var raw = rawSeries || [];
        for (var i = 0; i < raw.length; i++) {
            var series = raw[i] || {};
            var rawName = series.name || "";
            var displayName = source === "tools" ? rawName : providerDisplayName(rawName);
            var color = source === "tools" ? toolColor(rawName) : providerColor(rawName);

            out.push({
                rawName: rawName,
                name: displayName,
                color: color,
                points: series.points || [],
                latestValue: series.latestValue || 0,
                deltaPercent: series.deltaPercent || 0,
                sampleCount: series.sampleCount || 0
            });
        }
        return out;
    }

    function hasCompareData() {
        var series = fullRoot.compareSeriesData || [];
        for (var i = 0; i < series.length; i++) {
            if ((series[i].points || []).length > 0) return true;
        }
        return false;
    }

    function compareRanking() {
        var ranked = [];
        var series = fullRoot.compareSeriesData || [];
        for (var i = 0; i < series.length; i++) {
            if ((series[i].points || []).length === 0) continue;
            ranked.push({
                name: series[i].name,
                latestValue: series[i].latestValue || 0,
                deltaPercent: series[i].deltaPercent || 0,
                sampleCount: series[i].sampleCount || 0,
                color: series[i].color
            });
        }

        ranked.sort(function(a, b) {
            return b.latestValue - a.latestValue;
        });
        return ranked;
    }

    function formatCompareValue(value) {
        var metric = currentCompareMetric();
        if (metric === "cost") return "$" + value.toFixed(value < 1 ? 4 : 2);
        if (metric === "percentUsed" || metric === "rateLimitUsed") return Math.round(value) + "%";
        if (value >= 1000000) return (value / 1000000).toFixed(1) + "M";
        if (value >= 1000) return (value / 1000).toFixed(1) + "K";
        return Math.round(value).toString();
    }

    function canExportCurrentView() {
        if (fullRoot.historyLoading) return false;
        if (fullRoot.compareMode) {
            return hasCompareData();
        }
        return selectedDetailProviderDbName() !== "" && fullRoot.detailSnapshots.length > 0;
    }

    function exportCompareCsv(source, metric, series) {
        var lines = ["source,metric,name,timestamp,value,latest_value,delta_percent,sample_count"];
        for (var i = 0; i < series.length; i++) {
            var s = series[i];
            var points = s.points || [];
            for (var p = 0; p < points.length; p++) {
                lines.push([
                    source,
                    metric,
                    csvEscape(s.name),
                    csvEscape(points[p].timestamp || ""),
                    (points[p].value || 0).toString(),
                    (s.latestValue || 0).toString(),
                    (s.deltaPercent || 0).toString(),
                    (s.sampleCount || 0).toString()
                ].join(","));
            }
        }
        return lines.join("\n") + "\n";
    }

    function csvEscape(value) {
        var str = String(value || "");
        if (str.indexOf(",") >= 0 || str.indexOf("\"") >= 0 || str.indexOf("\n") >= 0) {
            return "\"" + str.replace(/\"/g, "\"\"") + "\"";
        }
        return str;
    }

    function exportData(format) {
        if (!root.usageDb) return;

        var data = "";

        if (fullRoot.compareMode) {
            var source = compareSourceCombo.currentValue || "providers";
            var metric = currentCompareMetric();
            var series = fullRoot.compareSeriesData || [];

            if (format === "csv") {
                data = exportCompareCsv(source, metric, series);
            } else {
                data = JSON.stringify({
                    mode: "compare",
                    source: source,
                    metric: metric,
                    from: fullRoot.lastQueryFrom.toISOString(),
                    to: fullRoot.lastQueryTo.toISOString(),
                    series: series
                }, null, 2);
            }
        } else {
            var providerDbName = selectedDetailProviderDbName();
            if (providerDbName === "") return;

            if (format === "csv") {
                data = root.usageDb.exportCsv(providerDbName, fullRoot.lastQueryFrom, fullRoot.lastQueryTo);
            } else {
                data = root.usageDb.exportJson(providerDbName, fullRoot.lastQueryFrom, fullRoot.lastQueryTo);
            }
        }

        if (data) {
            clipboard.setText(data);
        }
    }

    ClipboardHelper {
        id: clipboard
    }

    Component.onCompleted: {
        if (hasAnyProvider()) {
            plasmoid.configuration.setupWizardCompleted = true;
            plasmoid.configuration.setupWizardDismissed = false;
        }
        resetCompareMetric();
        refreshHistory();
    }
}
