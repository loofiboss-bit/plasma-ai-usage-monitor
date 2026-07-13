import QtQuick
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

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        visible: card.backend

                        SourceBadge {
                            text: sourceLabel(card.backend?.costSource ?? "unknown")
                        }

                        SourceBadge {
                            text: usageLabel(card.backend?.usageSource ?? "unknown")
                        }

                        SourceBadge {
                            visible: modelNeedsReview()
                            text: i18n("Review needed")
                        }

                        SourceBadge {
                            visible: modelSourceConflict()
                            text: i18n("Source conflict")
                        }

                        SourceBadge {
                            visible: modelUnknownPricing()
                            text: i18n("Unknown pricing")
                        }
                    }
                }

                // Collapsed Summary
                RowLayout {
                    visible: card.collapsed && card.showCost && (card.backend?.connected ?? false)
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.Label {
                        text: Utils.formatMoney(card.backend?.cost ?? 0, card.backend?.currency || "USD")
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
                        text: metricText("input_tokens", card.backend?.inputTokens ?? 0)
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
                        text: metricText("output_tokens", card.backend?.outputTokens ?? 0)
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
                        text: metricText("requests", card.backend?.requestCount ?? 0)
                        font.bold: true
                    }
                }
            }

            // Cost & Budgets
            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && card.showCost && (card.backend?.connected ?? false)
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: card.backend?.budgetCurrencyMismatch ?? false
                    text: i18n("Budget disabled: configured currency %1 does not match observed %2.",
                               card.backend?.budgetCurrency || "USD", card.backend?.currency || "")
                    color: Kirigami.Theme.neutralTextColor
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label {
                        text: costHeading()
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }
                    Item { Layout.fillWidth: true }
                    PlasmaComponents.Label {
                        text: Utils.formatMoney(card.backend?.cost ?? 0, card.backend?.currency || "USD")
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

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: (card.backend?.costSource ?? "") === "connectivity_probe"
                    text: i18n("Connection check only — this provider does not expose account usage. Cost is not billing spend.")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
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
                        text: Utils.formatMoney(card.backend?.balance ?? 0, card.backend?.currency || "USD")
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
                        visible: modelData.budget > 0 && !(card.backend?.budgetCurrencyMismatch ?? false)
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
                                text: Utils.formatMoney(modelData.cost, card.backend?.currency || "USD")
                                      + " / " + Utils.formatMoney(modelData.budget, card.backend?.currency || "USD")
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
                visible: !card.collapsed && (card.backend?.connected ?? false) && liveRateRows().length > 0
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
                    model: liveRateRows()
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

            ColumnLayout {
                Layout.fillWidth: true
                visible: !card.collapsed && publishedLimitRows().length > 0
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    text: i18n("Published caps — not live remaining quota")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.bold: true
                    opacity: 0.8
                }
                Repeater {
                    model: publishedLimitRows()
                    PlasmaComponents.Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.label + ": " + formatNumber(modelData.value)
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
                    text: i18n("Refresh models")
                    icon.name: "view-refresh"
                    onClicked: card.backend.refreshModelsNow()
                }
                PlasmaComponents.Button {
                    visible: typeof card.backend?.testConnectionNow === "function"
                    text: i18n("Test connection now — may consume quota or money")
                    icon.name: "network-connect"
                    onClicked: card.backend.testConnectionNow()
                }
                PlasmaComponents.Button {
                    visible: typeof card.backend?.countTokensDiagnostic === "function"
                    text: i18n("Run token diagnostic — may consume quota")
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
                            return i18n("Last success: %1 · last attempt: %2",
                                        success ? formatRelativeTime(success) : i18n("never"),
                                        attempt ? formatRelativeTime(attempt) : i18n("never"));
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

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: (card.backend?.probeRequestCount ?? 0) > 0
                    text: i18n("Connection checks: %1, probe tokens: %2",
                               card.backend?.probeRequestCount ?? 0,
                               formatNumber((card.backend?.probeInputTokens ?? 0) + (card.backend?.probeOutputTokens ?? 0)))
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: !!card.backend?.modelsLastDiscovered
                    text: i18n("Live models discovered: %1", formatRelativeTime(card.backend.modelsLastDiscovered))
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
                    visible: partialCapabilityText() !== ""
                    text: partialCapabilityText()
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.neutralTextColor
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: card.scheduler !== null && card.backend
                    text: i18n("Next refresh: %1%2",
                               nextRefreshText(),
                               refreshModeSuffix())
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: (card.backend?.consecutiveErrors ?? 0) > 0
                        ? Kirigami.Theme.neutralTextColor
                        : Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    function sourceLabel(source) {
        if (source === "billing_api") return i18n("Actual billing");
        if (source === "usage_api") return i18n("Actual usage");
        if (source === "actual_api") return i18n("Actual usage");
        if (source === "estimated_from_usage") return i18n("Estimated");
        if (source === "connectivity_probe") return i18n("Probe only");
        if (source === "self_tracked") return i18n("Self-tracked");
        if (source === "browser_sync") return i18n("Browser sync");
        return i18n("Unknown");
    }

    function liveRateRows() {
        if (!card.backend || typeof card.backend.metric !== "function") return [];
        var rows = [];
        var requestLimit = card.backend.metric("request_limit");
        var requestRemaining = card.backend.metric("request_remaining");
        if (requestLimit.available && requestRemaining.available && Number(requestLimit.value) > 0)
            rows.push({ label: i18n("Requests/min"), total: Number(requestLimit.value), remaining: Number(requestRemaining.value) });
        var tokenLimit = card.backend.metric("token_limit");
        var tokenRemaining = card.backend.metric("token_remaining");
        if (tokenLimit.available && tokenRemaining.available && Number(tokenLimit.value) > 0)
            rows.push({ label: i18n("Tokens/min"), total: Number(tokenLimit.value), remaining: Number(tokenRemaining.value) });
        return rows;
    }

    function publishedLimitRows() {
        var metrics = card.backend?.metrics || [];
        var rows = [];
        for (var i = 0; i < metrics.length; i++) {
            var metric = metrics[i];
            if (!metric.available || metric.quality !== "published_cap") continue;
            if (metric.kind === "request_limit")
                rows.push({ label: i18n("Published request cap"), value: Number(metric.value), window: metric.window || i18n("documented window") });
            else if (metric.kind === "token_limit")
                rows.push({ label: i18n("Published token cap"), value: Number(metric.value), window: metric.window || i18n("documented window") });
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
        return i18n("Partial data: %1 available; %2 unavailable",
                    available.join(", "), failed.join(", "));
    }

    function usageLabel(source) {
        if (source === "actual_api") return i18n("Actual usage");
        if (source === "usage_api") return i18n("Actual usage");
        if (source === "model_discovery_api" || source === "connectivity_read_only") return i18n("Connectivity only");
        if (source === "connectivity_probe") return i18n("Probe only");
        if (source === "self_tracked") return i18n("Self-tracked");
        if (source === "browser_sync") return i18n("Browser sync");
        if (source === "estimated_from_usage") return i18n("Estimated");
        return i18n("Unknown");
    }

    function metricText(kind, legacyValue) {
        var rows = card.backend?.metrics || [];
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].kind === kind) {
                return rows[i].available ? formatNumber(rows[i].value) : i18n("Unknown");
            }
        }
        var source = card.backend?.usageSource || "unknown";
        if (source === "unknown" || source === "connectivity_probe"
                || source === "model_discovery_api" || source === "connectivity_read_only") {
            return i18n("Unknown");
        }
        return formatNumber(legacyValue);
    }

    function costHeading() {
        var source = card.backend?.costSource ?? "unknown";
        if (source === "billing_api") return i18n("API Spend");
        if (source === "estimated_from_usage") return i18n("Estimated Burn");
        if (source === "connectivity_probe") return i18n("Probe Cost");
        return (card.backend?.isEstimatedCost ?? false) ? i18n("Estimated Cost") : i18n("Cost");
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

    function rateLimitColor(remaining, total) { return Utils.rateLimitColor(remaining, total, Kirigami.Theme); }
    function budgetColor(spent, budget) { return Utils.budgetColor(spent, budget, Kirigami.Theme); }
    function formatNumber(n) { return Utils.formatNumber(n); }
    function formatRelativeTime(dateTime) { return Utils.formatRelativeTime(dateTime, Qt, i18n); }

    function nextRefreshText() {
        if (!card.scheduler || !card.modelData || !card.backend) return i18n("unknown");
        var last = card.backend.lastRefreshed;
        if (!last) return i18n("on next timer");
        var lastDate = new Date(last);
        var interval = card.scheduler.scheduledInterval(card.modelData) || 0;
        var ms = lastDate.getTime() + interval - Date.now();
        if (ms <= 0) return i18n("due now");
        var seconds = Math.ceil(ms / 1000);
        if (seconds < 60) return i18n("in %1 sec", seconds);
        var minutes = Math.ceil(seconds / 60);
        if (minutes < 60) return i18n("in %1 min", minutes);
        return i18n("in %1 h", Math.ceil(minutes / 60));
    }

    function refreshModeSuffix() {
        if (!card.scheduler || !card.modelData) return "";
        var parts = [];
        if (card.scheduler.popupOpen) parts.push(i18n("popup fast-refresh"));
        if (card.scheduler.backoffMultiplier(card.modelData) > 1) parts.push(i18n("backoff active"));
        return parts.length > 0 ? " (" + parts.join(", ") + ")" : "";
    }
}
