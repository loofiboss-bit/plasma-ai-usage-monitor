import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

ColumnLayout {
    id: card

    required property var modelData
    required property string providerName
    required property string providerIcon
    required property string providerColor
    required property var backend
    property bool showCost: false
    property bool showUsage: false
    property bool collapsed: false
    readonly property bool narrowCard: card.width < Kirigami.Units.gridUnit * 14
    readonly property bool compactDetails: card.width < Kirigami.Units.gridUnit * 13

    spacing: 0

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: cardContent.implicitHeight + Kirigami.Units.largeSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: {
            if (card.backend?.error) return Qt.alpha(Kirigami.Theme.negativeTextColor, 0.3);
            if (card.backend?.connected) return Qt.alpha(card.providerColor, 0.3);
            return Qt.alpha(Kirigami.Theme.disabledTextColor, 0.2);
        }

        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: Kirigami.Units.cornerRadius
            color: card.backend?.error ? Kirigami.Theme.negativeTextColor : card.providerColor
            opacity: card.backend?.connected || card.backend?.error ? 0.8 : 0.2
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

        clip: true

        ColumnLayout {
            id: cardContent
            anchors {
                fill: parent
                margins: Kirigami.Units.largeSpacing
                leftMargin: Kirigami.Units.largeSpacing + 4
            }
            spacing: Kirigami.Units.mediumSpacing

            // Header Section
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: card.providerIcon
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    color: card.backend?.error ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    PlasmaExtras.Heading {
                        level: 4
                        text: card.providerName
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    PlasmaComponents.Label {
                        text: {
                            if (!card.backend) return i18n("Not Available");
                            if (card.backend.loading) return i18n("Loading...");
                            if (card.backend.error) return i18n("Error");
                            if (card.backend.connected) {
                                let parts = [i18n("Connected")];
                                if (card.backend.model) parts.push(card.backend.model);
                                return parts.join(" • ");
                            }
                            return i18n("Disconnected");
                        }
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: card.backend?.error ? Kirigami.Theme.negativeTextColor : Qt.alpha(Kirigami.Theme.textColor, 0.7)
                        elide: Text.ElideRight
                    }
                }

                // Collapsed Summary
                RowLayout {
                    visible: card.collapsed && card.showCost && (card.backend?.connected ?? false)
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.Label {
                        text: "$" + (card.backend?.cost ?? 0).toFixed(2)
                        font.bold: true
                        color: Kirigami.Theme.textColor
                    }
                }

                PlasmaComponents.ToolButton {
                    activeFocusOnTab: true
                    icon.name: card.collapsed ? "arrow-down" : "arrow-up"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    onClicked: card.collapsed = !card.collapsed
                    PlasmaComponents.ToolTip { text: card.collapsed ? i18n("Expand") : i18n("Collapse") }
                }
            }

            // Error message (expandable)
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && (card.backend?.error ?? "") !== ""
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: errorRow.implicitHeight + Kirigami.Units.smallSpacing * 2
                    radius: Kirigami.Units.smallSpacing
                    color: Qt.alpha(Kirigami.Theme.negativeBackgroundColor, 0.3)
                    border.width: 1
                    border.color: Qt.alpha(Kirigami.Theme.negativeTextColor, 0.3)

                    RowLayout {
                        id: errorRow
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: humanizeError(card.backend?.error ?? "")
                            color: Kirigami.Theme.negativeTextColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.WordWrap
                        }

                        PlasmaComponents.ToolButton {
                            activeFocusOnTab: true
                            icon.name: "view-refresh"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            PlasmaComponents.ToolTip { text: i18n("Retry") }
                            onClicked: if (card.backend) card.backend.refresh()
                        }
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                visible: !card.collapsed && (card.backend?.connected ?? false)
            }

            // Metrics & Usage Grid
            GridLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && card.showUsage && (card.backend?.connected ?? false)
                columns: card.narrowCard ? 1 : 3
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    spacing: 0
                    PlasmaComponents.Label {
                        text: i18n("Input Tokens")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    PlasmaComponents.Label {
                        text: formatNumber(card.backend?.inputTokens ?? 0)
                        font.bold: true
                    }
                }

                ColumnLayout {
                    spacing: 0
                    PlasmaComponents.Label {
                        text: i18n("Output Tokens")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    PlasmaComponents.Label {
                        text: formatNumber(card.backend?.outputTokens ?? 0)
                        font.bold: true
                    }
                }

                ColumnLayout {
                    spacing: 0
                    PlasmaComponents.Label {
                        text: i18n("Requests")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    PlasmaComponents.Label {
                        text: formatNumber(card.backend?.requestCount ?? 0)
                        font.bold: true
                    }
                }
            }

            // Cost & Budgets
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && card.showCost && (card.backend?.connected ?? false)
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        text: (card.backend?.isEstimatedCost ?? false) ? i18n("Estimated Cost") : i18n("Cost")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    Item { Layout.fillWidth: true }
                    PlasmaComponents.Label {
                        text: "$" + (card.backend?.cost ?? 0).toFixed(4)
                        font.bold: true
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                        color: {
                            var c = card.backend?.cost ?? 0;
                            if (c > 10) return Kirigami.Theme.negativeTextColor;
                            if (c > 5) return Kirigami.Theme.neutralTextColor;
                            return Kirigami.Theme.textColor;
                        }
                    }
                }

                // DeepSeek Balance
                RowLayout {
                    Layout.fillWidth: true
                    visible: card.providerName === "DeepSeek" && (card.backend?.balance ?? 0) > 0
                    PlasmaComponents.Label {
                        text: i18n("Balance")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    Item { Layout.fillWidth: true }
                    PlasmaComponents.Label {
                        text: "$" + (card.backend?.balance ?? 0).toFixed(2)
                        font.bold: true
                        color: (card.backend?.balance ?? 0) < 5 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
                    }
                }

                // Budgets
                Repeater {
                    model: [
                        { label: i18n("Daily Budget"), cost: card.backend?.dailyCost ?? 0, budget: card.backend?.dailyBudget ?? 0 },
                        { label: i18n("Monthly Budget"), cost: card.backend?.monthlyCost ?? 0, budget: card.backend?.monthlyBudget ?? 0 }
                    ]
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: modelData.budget > 0
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            PlasmaComponents.Label {
                                text: modelData.label
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.7
                            }
                            Item { Layout.fillWidth: true }
                            PlasmaComponents.Label {
                                text: "$" + modelData.cost.toFixed(2) + " / $" + modelData.budget.toFixed(2)
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }
                        QQC2.ProgressBar {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 4
                            from: 0; to: modelData.budget
                            value: Math.min(modelData.cost, modelData.budget)
                            background: Rectangle { implicitHeight: 4; radius: 2; color: Qt.alpha(Kirigami.Theme.textColor, 0.1) }
                            contentItem: Rectangle {
                                width: parent.visualPosition * parent.width
                                height: 4; radius: 2
                                color: budgetColor(modelData.cost, modelData.budget)
                                Behavior on width { NumberAnimation { duration: 300 } }
                            }
                        }
                    }
                }
            }

            // Rate Limits
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && (card.backend?.connected ?? false) && ((card.backend?.rateLimitRequests ?? 0) > 0 || (card.backend?.rateLimitTokens ?? 0) > 0)
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        text: i18n("Rate Limits")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        font.bold: true
                        opacity: 0.8
                    }
                    Item { Layout.fillWidth: true }
                    PlasmaComponents.Label {
                        visible: (card.backend?.rateLimitResetTime ?? "") !== ""
                        text: i18n("Resets: %1", card.backend?.rateLimitResetTime)
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.5
                    }
                }

                Repeater {
                    model: [
                        { label: i18n("Requests/min"), total: card.backend?.rateLimitRequests ?? 0, remaining: card.backend?.rateLimitRequestsRemaining ?? 0 },
                        { label: i18n("Tokens/min"), total: card.backend?.rateLimitTokens ?? 0, remaining: card.backend?.rateLimitTokensRemaining ?? 0 }
                    ]
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: modelData.total > 0
                        spacing: 2
                        readonly property int used: modelData.total - modelData.remaining
                        RowLayout {
                            Layout.fillWidth: true
                            PlasmaComponents.Label {
                                text: modelData.label
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.7
                            }
                            Item { Layout.fillWidth: true }
                            PlasmaComponents.Label {
                                text: formatNumber(parent.used) + " / " + formatNumber(modelData.total)
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }
                        QQC2.ProgressBar {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 4
                            from: 0; to: modelData.total
                            value: parent.used
                            background: Rectangle { implicitHeight: 4; radius: 2; color: Qt.alpha(Kirigami.Theme.textColor, 0.1) }
                            contentItem: Rectangle {
                                width: parent.visualPosition * parent.width
                                height: 4; radius: 2
                                color: rateLimitColor(modelData.remaining, modelData.total)
                                Behavior on width { NumberAnimation { duration: 300 } }
                            }
                        }
                    }
                }
            }

            // Footer
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && card.backend
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: {
                            var lr = card.backend?.lastRefreshed;
                            if (!lr) return i18n("Last refresh: never");
                            return i18n("Last refresh: %1", formatRelativeTime(lr));
                        }
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.5
                    }

                    PlasmaComponents.Label {
                        text: i18n("Failures: %1/%2", card.backend?.consecutiveErrors ?? 0, card.backend?.errorCount ?? 0)
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: (card.backend?.consecutiveErrors ?? 0) > 0
                            ? Kirigami.Theme.neutralTextColor
                            : Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
    }

    function humanizeError(raw) {
        if (!raw) return "";
        const s = raw.toString();
        if (s.includes("403") && card.providerName === "OpenAI")
            return s + "\nHint: OpenAI requires an Admin API key.";
        if (s.includes("403") || s.includes("401"))
            return s + "\nHint: Check your API key in Settings \u2192 Providers";
        if (s.includes("429"))
            return s + "\nHint: Rate limit hit. Try a longer refresh interval.";
        if (s.includes("NetworkError") || s.includes("network error") || s.includes("host not found") || s.includes("Connection refused"))
            return s + "\nHint: Cannot reach the API. Check your internet connection.";
        if (s.includes("KWallet"))
            return s + "\nHint: Enable KWallet in System Settings \u2192 KDE Wallet";
        return s;
    }

    function rateLimitColor(remaining, total) { return Utils.rateLimitColor(remaining, total, Kirigami.Theme); }
    function budgetColor(spent, budget) { return Utils.budgetColor(spent, budget, Kirigami.Theme); }
    function formatNumber(n) { return Utils.formatNumber(n); }
    function formatRelativeTime(dateTime) { return Utils.formatRelativeTime(dateTime, Qt, i18n); }
}
