pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "Utils.js" as Utils

ColumnLayout {
    id: card

    required property var modelData
    required property string providerName
    required property string providerIcon
    required property string providerColor
    required property var backend
    property var readiness: ({})
    property var scheduler: null
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
                            if (!card.backend) return KI18n.i18n("Not Available");
                            if (card.backend.loading) return KI18n.i18n("Loading...");
                            if (card.readiness.readinessStateKey === "reporting_actual")
                                return card.readiness.qualityClass === "balance"
                                    ? KI18n.i18n("Reporting provider balance") : KI18n.i18n("Reporting actual data");
                            if (card.readiness.readinessStateKey === "reporting_estimate")
                                return KI18n.i18n("Reporting estimated or local data");
                            if (card.readiness.readinessStateKey === "connected_connectivity_only")
                                return KI18n.i18n("Connectivity verified only");
                            if (card.backend.error) return KI18n.i18n("Needs attention");
                            if (card.backend.connected) return KI18n.i18n("Connected");
                            return KI18n.i18n("Disconnected");
                        }
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: card.backend?.error ? Kirigami.Theme.negativeTextColor : Qt.alpha(Kirigami.Theme.textColor, 0.7)
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        visible: card.backend

                        Repeater {
                            model: card.outcomeBadges()
                            SourceBadge {
                                required property string modelData
                                text: card.badgeLabel(modelData)
                            }
                        }

                        SourceBadge {
                            visible: card.modelNeedsReview()
                            text: KI18n.i18n("Review needed")
                        }

                        SourceBadge {
                            visible: card.modelSourceConflict()
                            text: KI18n.i18n("Source conflict")
                        }

                        SourceBadge {
                            visible: card.modelUnknownPricing()
                            text: KI18n.i18n("Unknown pricing")
                        }
                    }
                }

                // Collapsed Summary
                RowLayout {
                    visible: card.collapsed && card.showCost && (card.backend?.connected ?? false)
                             && card.hasAvailableCostData()
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.Label {
                        text: card.currentCostText()
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
                    Accessible.name: card.collapsed
                        ? KI18n.i18n("Expand %1 details", card.providerName)
                        : KI18n.i18n("Collapse %1 details", card.providerName)
                    onClicked: card.collapsed = !card.collapsed
                    PlasmaComponents.ToolTip { text: card.collapsed ? KI18n.i18n("Expand") : KI18n.i18n("Collapse") }
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
                            text: card.humanizeError(card.backend?.error ?? "")
                            color: Kirigami.Theme.negativeTextColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.WordWrap
                        }

                        PlasmaComponents.ToolButton {
                            activeFocusOnTab: true
                            icon.name: "view-refresh"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            Accessible.name: KI18n.i18n("Retry %1", card.providerName)
                            PlasmaComponents.ToolTip { text: KI18n.i18n("Retry") }
                            onClicked: if (card.backend) card.backend.requestRefresh(3)
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
                         && card.hasAvailableUsageMetrics()
                columns: card.narrowCard ? 1 : 3
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    visible: card.metricAvailable("input_tokens")
                    spacing: 0
                    PlasmaComponents.Label {
                        text: KI18n.i18n("Input Tokens")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    PlasmaComponents.Label {
                        text: card.metricText("input_tokens", card.backend?.inputTokens ?? 0)
                        font.bold: true
                    }
                }

                ColumnLayout {
                    visible: card.metricAvailable("output_tokens")
                    spacing: 0
                    PlasmaComponents.Label {
                        text: KI18n.i18n("Output Tokens")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    PlasmaComponents.Label {
                        text: card.metricText("output_tokens", card.backend?.outputTokens ?? 0)
                        font.bold: true
                    }
                }

                ColumnLayout {
                    visible: card.metricAvailable("requests")
                    spacing: 0
                    PlasmaComponents.Label {
                        text: KI18n.i18n("Requests")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    PlasmaComponents.Label {
                        text: card.metricText("requests", card.backend?.requestCount ?? 0)
                        font.bold: true
                    }
                }
            }

            // Cost & Budgets
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && card.showCost && (card.backend?.connected ?? false)
                         && (card.hasAvailableCostData() || card.balanceRows().length > 0)
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: card.backend?.budgetCurrencyMismatch ?? false
                    text: KI18n.i18n("Budget disabled: configured currency %1 does not match observed %2.",
                               card.backend?.budgetCurrency || "USD", card.backend?.currency || "")
                    color: Kirigami.Theme.neutralTextColor
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: card.hasAvailableCostData()
                    PlasmaComponents.Label {
                        text: card.costHeading()
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    Item { Layout.fillWidth: true }
                    PlasmaComponents.Label {
                        text: card.currentCostText()
                        font.bold: true
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                        color: {
                            if (!card.currentCostAvailable()) return Kirigami.Theme.disabledTextColor;
                            var c = card.currentCostValue();
                            if (c > 10) return Kirigami.Theme.negativeTextColor;
                            if (c > 5) return Kirigami.Theme.neutralTextColor;
                            return Kirigami.Theme.textColor;
                        }
                    }
                }

                Repeater {
                    model: card.balanceRows()
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        PlasmaComponents.Label {
                            text: KI18n.i18n("Available balance")
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }
                        Item { Layout.fillWidth: true }
                        PlasmaComponents.Label {
                            text: Utils.formatMoney(card.modelData.value, card.modelData.currency)
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }
                    }
                }

                // Budgets
                Repeater {
                    model: [
                        { label: KI18n.i18n("Daily Budget"), cost: card.backend?.dailyCost ?? 0, budget: card.backend?.dailyBudget ?? 0 },
                        { label: KI18n.i18n("Monthly Budget"), cost: card.backend?.monthlyCost ?? 0, budget: card.backend?.monthlyBudget ?? 0 }
                    ]
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: card.hasAvailableCostData() && card.modelData.budget > 0
                                 && !(card.backend?.budgetCurrencyMismatch ?? false)
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            PlasmaComponents.Label {
                                text: card.modelData.label
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.7
                            }
                            Item { Layout.fillWidth: true }
                            PlasmaComponents.Label {
                                text: Utils.formatMoney(card.modelData.cost, card.backend?.currency || "USD")
                                      + " / " + Utils.formatMoney(card.modelData.budget, card.backend?.currency || "USD")
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }
                        QQC2.ProgressBar {
                            id: budgetProgress
                            Layout.fillWidth: true
                            Layout.preferredHeight: 4
                            from: 0; to: card.modelData.budget
                            value: Math.min(card.modelData.cost, card.modelData.budget)
                            background: Rectangle { implicitHeight: 4; radius: 2; color: Qt.alpha(Kirigami.Theme.textColor, 0.1) }
                            contentItem: Rectangle {
                                width: budgetProgress.visualPosition
                                       * budgetProgress.width
                                height: 4; radius: 2
                                color: card.budgetColor(card.modelData.cost, card.modelData.budget)
                                Behavior on width { NumberAnimation { duration: 300 } }
                            }
                        }
                    }
                }
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                visible: !card.collapsed && (card.backend?.connected ?? false)
                         && !card.hasAvailableUsageMetrics()
                         && !card.hasAvailableCostData()
                         && card.balanceRows().length === 0
                text: card.readiness.readinessStateKey === "connected_connectivity_only"
                    ? KI18n.i18n("Connection verified. This source does not expose compatible token, spend, or balance metrics.")
                    : KI18n.i18n("No compatible usage or spend metric is available yet. Refresh the source or review its permissions.")
                color: Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                wrapMode: Text.WordWrap
            }

            // Rate Limits
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && (card.backend?.connected ?? false) && card.liveRateRows().length > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        text: KI18n.i18n("Rate Limits")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        font.bold: true
                        opacity: 0.8
                    }
                    Item { Layout.fillWidth: true }
                    PlasmaComponents.Label {
                        visible: (card.backend?.rateLimitResetTime ?? "") !== ""
                        text: KI18n.i18n("Resets: %1", card.backend?.rateLimitResetTime)
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.5
                    }
                }

                Repeater {
                    model: card.liveRateRows()
                    ColumnLayout {
                        id: rateRow
                        Layout.fillWidth: true
                        visible: card.modelData.total > 0
                        spacing: 2
                        readonly property int used: card.modelData.total - card.modelData.remaining
                        RowLayout {
                            Layout.fillWidth: true
                            PlasmaComponents.Label {
                                text: card.modelData.label
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.7
                            }
                            Item { Layout.fillWidth: true }
                            PlasmaComponents.Label {
                                text: card.formatNumber(rateRow.used) + " / " + card.formatNumber(card.modelData.total)
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }
                        QQC2.ProgressBar {
                            id: rateProgress
                            Layout.fillWidth: true
                            Layout.preferredHeight: 4
                            from: 0; to: card.modelData.total
                            value: parent.used
                            background: Rectangle { implicitHeight: 4; radius: 2; color: Qt.alpha(Kirigami.Theme.textColor, 0.1) }
                            contentItem: Rectangle {
                                width: rateProgress.visualPosition
                                       * rateProgress.width
                                height: 4; radius: 2
                                color: card.rateLimitColor(card.modelData.remaining, card.modelData.total)
                                Behavior on width { NumberAnimation { duration: 300 } }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && card.publishedLimitRows().length > 0
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    text: KI18n.i18n("Published caps — not live remaining quota")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.bold: true
                    opacity: 0.8
                }
                Repeater {
                    model: card.publishedLimitRows()
                    PlasmaComponents.Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.label + ": " + card.formatNumber(modelData.value)
                              + " / " + modelData.window
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && ((typeof card.backend?.testConnectionNow === "function")
                         || (typeof card.backend?.countTokensDiagnostic === "function")
                         || (typeof card.backend?.refreshModelsNow === "function"))

                PlasmaComponents.Button {
                    visible: typeof card.backend?.refreshModelsNow === "function"
                    text: KI18n.i18n("Refresh models")
                    icon.name: "view-refresh"
                    onClicked: card.backend.refreshModelsNow()
                }
                PlasmaComponents.Button {
                    visible: typeof card.backend?.testConnectionNow === "function"
                    text: KI18n.i18n("Test connection now — may consume quota or money")
                    icon.name: "network-connect"
                    onClicked: card.backend.testConnectionNow()
                }
                PlasmaComponents.Button {
                    visible: typeof card.backend?.countTokensDiagnostic === "function"
                    text: KI18n.i18n("Run token diagnostic — may consume quota")
                    icon.name: "tools-check-spelling"
                    onClicked: card.backend.countTokensDiagnostic()
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
                            var success = card.backend?.lastSuccess;
                            var attempt = card.backend?.lastAttempt;
                            return KI18n.i18n("Last success: %1 · last attempt: %2",
                                        success ? card.formatRelativeTime(success) : KI18n.i18n("never"),
                                        attempt ? card.formatRelativeTime(attempt) : KI18n.i18n("never"));
                        }
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.5
                    }

                    PlasmaComponents.Label {
                        text: KI18n.i18n("Failures: %1/%2", card.backend?.consecutiveErrors ?? 0, card.backend?.errorCount ?? 0)
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: (card.backend?.consecutiveErrors ?? 0) > 0
                            ? Kirigami.Theme.neutralTextColor
                            : Kirigami.Theme.disabledTextColor
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: (card.backend?.probeRequestCount ?? 0) > 0
                    text: KI18n.i18n("Connection checks: %1, probe tokens: %2",
                               card.backend?.probeRequestCount ?? 0,
                               card.formatNumber((card.backend?.probeInputTokens ?? 0) + (card.backend?.probeOutputTokens ?? 0)))
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: !!card.backend?.modelsLastDiscovered
                    text: KI18n.i18n("Live models discovered: %1", card.formatRelativeTime(card.backend.modelsLastDiscovered))
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: (card.backend?.selectedModelWarning || "") !== ""
                    text: card.backend?.selectedModelWarning || ""
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.neutralTextColor
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: card.partialCapabilityText() !== ""
                    text: card.partialCapabilityText()
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.neutralTextColor
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: card.scheduler !== null && card.backend
                    text: KI18n.i18n("Next refresh: %1%2",
                               card.nextRefreshText(),
                               card.refreshModeSuffix())
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: (card.backend?.consecutiveErrors ?? 0) > 0
                        ? Kirigami.Theme.neutralTextColor
                        : Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    function liveRateRows() {
        if (!card.backend || typeof card.backend.metric !== "function") return [];
        var rows = [];
        var requestLimit = card.backend.metric("request_limit");
        var requestRemaining = card.backend.metric("request_remaining");
        var requestLimitValue = Number(requestLimit.value);
        var requestRemainingValue = Number(requestRemaining.value);
        if (requestLimit.available && requestRemaining.available
                && Number.isFinite(requestLimitValue) && requestLimitValue > 0
                && Number.isFinite(requestRemainingValue))
            rows.push({ label: KI18n.i18n("Requests/min"), total: requestLimitValue, remaining: requestRemainingValue });
        var tokenLimit = card.backend.metric("token_limit");
        var tokenRemaining = card.backend.metric("token_remaining");
        var tokenLimitValue = Number(tokenLimit.value);
        var tokenRemainingValue = Number(tokenRemaining.value);
        if (tokenLimit.available && tokenRemaining.available
                && Number.isFinite(tokenLimitValue) && tokenLimitValue > 0
                && Number.isFinite(tokenRemainingValue))
            rows.push({ label: KI18n.i18n("Tokens/min"), total: tokenLimitValue, remaining: tokenRemainingValue });
        return rows;
    }

    function publishedLimitRows() {
        var metrics = card.backend?.metrics || [];
        var rows = [];
        for (var i = 0; i < metrics.length; i++) {
            var metric = metrics[i];
            if (!metric.available || metric.quality !== "published_cap") continue;
            if (metric.kind === "request_limit")
                rows.push({ label: KI18n.i18n("Published request cap"), value: Number(metric.value), window: metric.window || KI18n.i18n("documented window") });
            else if (metric.kind === "token_limit")
                rows.push({ label: KI18n.i18n("Published token cap"), value: Number(metric.value), window: metric.window || KI18n.i18n("documented window") });
        }
        return rows;
    }

    function partialCapabilityText() {
        var statuses = card.backend?.capabilityStatus || {};
        var available = [];
        var failed = [];
        var keys = Object.keys(statuses);
        for (var i = 0; i < keys.length; i++) {
            var row = statuses[keys[i]] || {};
            if (row.status === "available") available.push(keys[i]);
            else if (row.status === "failed" || row.status === "unavailable") failed.push(keys[i]);
        }
        if (available.length === 0 || failed.length === 0) return "";
        return KI18n.i18n("Partial data: %1 available; %2 unavailable",
                    available.join(", "), failed.join(", "));
    }

    function outcomeBadges() {
        return Utils.providerOutcomeBadgeKeys(
            card.backend?.usageSource ?? "unknown",
            card.backend?.costSource ?? "unknown",
            card.readiness.monitoringLevel || card.modelData?.monitoringLevel || "",
            card.readiness.qualityClass || "");
    }

    function badgeLabel(key) {
        if (key === "gateway_usage") return KI18n.i18n("Gateway usage");
        if (key === "key_usage") return KI18n.i18n("API key usage");
        if (key === "provider_usage") return KI18n.i18n("Provider usage");
        if (key === "gateway_spend") return KI18n.i18n("Gateway-reported spend");
        if (key === "provider_spend") return KI18n.i18n("Provider-reported spend");
        if (key === "estimated_usage") return KI18n.i18n("Estimated usage");
        if (key === "estimated_cost") return KI18n.i18n("Estimated cost");
        if (key === "self_tracked") return KI18n.i18n("Self-tracked");
        if (key === "browser_sync") return KI18n.i18n("Browser sync");
        if (key === "provider_balance") return KI18n.i18n("Provider balance");
        return KI18n.i18n("Connectivity only");
    }

    function metricRow(kind) {
        var rows = card.backend?.metrics || [];
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind) return rows[i];
        }
        return ({});
    }

    function metricAvailable(kind) {
        var row = metricRow(kind);
        if (row.kind !== undefined) return row.available === true && Number.isFinite(Number(row.value));
        if ((card.backend?.metrics || []).length > 0) return false;
        var source = card.backend?.usageSource || "unknown";
        var compatible = ["actual_api", "usage_api", "estimated_from_usage", "self_tracked", "browser_sync"];
        return compatible.indexOf(source) >= 0;
    }

    function hasAvailableUsageMetrics() {
        return metricAvailable("input_tokens") || metricAvailable("output_tokens")
            || metricAvailable("requests");
    }

    function hasAvailableCostData() {
        return Utils.hasCompatibleCostData(card.backend);
    }

    function balanceRows() {
        var metrics = card.backend?.metrics || [];
        var rows = [];
        for (var i = 0; i < metrics.length; i++) {
            var metric = metrics[i];
            if (metric.kind === "credit_balance" && metric.available === true
                    && Number.isFinite(Number(metric.value))) {
                rows.push({ value: Number(metric.value), currency: metric.currency || "USD" });
            }
        }
        return rows;
    }

    function metricText(kind, legacyValue) {
        var row = metricRow(kind);
        if (row.kind !== undefined) return row.available ? card.formatNumber(row.value) : "";
        if ((card.backend?.metrics || []).length > 0) return "";
        var source = card.backend?.usageSource || "unknown";
        if (source === "unknown" || source === "connectivity_probe"
                || source === "model_discovery_api" || source === "connectivity_read_only") {
            return "";
        }
        return card.formatNumber(legacyValue);
    }

    function costHeading() {
        var source = card.backend?.costSource ?? "unknown";
        if (source === "billing_api" || source === "usage_api" || source === "actual_api") return KI18n.i18n("API Spend");
        if (source === "estimated_from_usage") return KI18n.i18n("Estimated Burn");
        if (source === "connectivity_probe") return KI18n.i18n("Probe Cost");
        return (card.backend?.isEstimatedCost ?? false) ? KI18n.i18n("Estimated Cost") : KI18n.i18n("Cost");
    }

    function currentCostMetric() {
        var metrics = card.backend?.metrics || [];
        for (var i = metrics.length - 1; i >= 0; i--) {
            var metric = metrics[i] || {};
            if (metric["kind"] === "cost" && metric["window"] === "current")
                return metric;
        }
        return {};
    }

    function currentCostAvailable() {
        var metric = currentCostMetric();
        if (metric["available"] === true && Number.isFinite(Number(metric["value"]))) return true;
        return hasAvailableCostData();
    }

    function currentCostValue() {
        var metric = currentCostMetric();
        if (metric["available"] === true && Number.isFinite(Number(metric["value"])))
            return Number(metric["value"]);
        return hasAvailableCostData() ? Number(card.backend?.cost || 0) : 0;
    }

    function currentCostText() {
        if (!card.currentCostAvailable()) return "";
        var metric = currentCostMetric();
        return Utils.formatMoney(card.currentCostValue(), metric["currency"] || card.backend?.currency || "USD");
    }

    function currentCatalogModel() {
        if (!card.backend || !card.modelData) return ({});
        var providerKey = card.modelData.catalogKey || card.modelData.configKey || "";
        var modelId = card.backend.model || card.modelData.model || "";
        if (providerKey === "" || modelId === "") return ({});
        return ProviderPricingCatalog.model(providerKey, modelId) || ({});
    }

    function modelNeedsReview() {
        var row = currentCatalogModel();
        return row.needsManualReview === true;
    }

    function modelSourceConflict() {
        var row = currentCatalogModel();
        return row.sourceConflict === true;
    }

    function modelUnknownPricing() {
        var row = currentCatalogModel();
        var pricing = row.pricing || {};
        return pricing.precision === "unknown" || row.dataQuality === "unknown_pricing";
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

    function rateLimitColor(remaining, total) { return Utils.card.rateLimitColor(remaining, total, Kirigami.Theme); }
    function budgetColor(spent, budget) { return Utils.card.budgetColor(spent, budget, Kirigami.Theme); }
    function formatNumber(n) { return Utils.card.formatNumber(n); }
    function formatRelativeTime(dateTime) { return Utils.card.formatRelativeTime(dateTime, Qt, KI18n.i18n); }

    function nextRefreshText() {
        if (!card.scheduler || !card.modelData || !card.backend) return KI18n.i18n("unknown");
        var last = card.backend.lastRefreshed;
        if (!last) return KI18n.i18n("on next timer");
        var lastDate = new Date(last);
        var interval = card.scheduler.scheduledInterval(card.modelData) || 0;
        var ms = lastDate.getTime() + interval - Date.now();
        if (ms <= 0) return KI18n.i18n("due now");
        var seconds = Math.ceil(ms / 1000);
        if (seconds < 60) return KI18n.i18n("in %1 sec", seconds);
        var minutes = Math.ceil(seconds / 60);
        if (minutes < 60) return KI18n.i18n("in %1 min", minutes);
        return KI18n.i18n("in %1 h", Math.ceil(minutes / 60));
    }

    function refreshModeSuffix() {
        if (!card.scheduler || !card.modelData) return "";
        var parts = [];
        if (card.scheduler.popupOpen) parts.push(KI18n.i18n("popup fast-refresh"));
        if (card.scheduler.backoffMultiplier(card.modelData) > 1) parts.push(KI18n.i18n("backoff active"));
        return parts.length > 0 ? " (" + parts.join(", ") + ")" : "";
    }
}
