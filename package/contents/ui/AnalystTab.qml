import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor as AIUsage
import "Utils.js" as Utils

Kirigami.ScrollablePage {
    id: analystPage

    title: i18n("The Analyst")

    property var activityData: []
    property double maxIntensity: 1.0
    property double avgEfficiency: 0.0
    property bool hasEfficiencyData: false
    property var overview: ({})

    readonly property var db: plasmoid.configuration.historyEnabled ? root.usageDb : null

    function enabledProviderEntries() {
        var entries = [];
        var providers = root.allProviders || [];
        for (var i = 0; i < providers.length; ++i) {
            if (providers[i].enabled) {
                entries.push({
                    label: providers[i].name,
                    entry: providers[i]
                });
            }
        }
        return entries;
    }

    function selectedProviderEntry() {
        var entries = enabledProviderEntries();
        if (entries.length === 0) {
            return null;
        }
        var idx = diagnosticsProviderCombo.currentIndex;
        if (idx < 0 || idx >= entries.length) {
            idx = 0;
        }
        return entries[idx].entry;
    }

    function refreshData() {
        if (!db || typeof db.getYearlyActivity !== "function"
                || typeof db.getEfficiencySeries !== "function"
                || typeof db.getAnalystOverview !== "function") {
            activityData = [];
            overview = ({});
            avgEfficiency = 0.0;
            hasEfficiencyData = false;
            return;
        }

        var activity = db.getYearlyActivity(plasmoid.configuration.analystIntensityMode);
        activityData = activity.days || [];
        maxIntensity = activity.maxIntensity || 1.0;

        var efficiency = db.getEfficiencySeries(30);
        avgEfficiency = 0.0;
        hasEfficiencyData = efficiency.length > 0;
        if (efficiency.length > 0) {
            var sum = 0;
            for (var i = 0; i < efficiency.length; ++i) {
                sum += Number(efficiency[i].value);
            }
            avgEfficiency = sum / efficiency.length;
        }

        overview = db.getAnalystOverview(30) || ({});
    }

    function formatCurrency(value) {
        if (overview.mixedCurrencies) return i18n("Mixed currencies");
        return Utils.formatMoney(value || 0, overview.currency || "USD");
    }

    function formatPercent(value) {
        var numeric = Number(value || 0);
        var prefix = numeric > 0 ? "+" : "";
        return prefix + numeric.toFixed(1) + "%";
    }

    function formatDateLabel(value) {
        if (!value) {
            return "";
        }
        return Qt.formatDate(new Date(value + "T00:00:00"), "MMM d");
    }

    function relativeRefresh(entry) {
        if (!entry || !entry.backend || !entry.backend.lastRefreshed) {
            return i18n("Never");
        }
        return Utils.formatRelativeTime(new Date(entry.backend.lastRefreshed));
    }

    function authConfigured(entry) {
        if (!entry || !entry.backend) {
            return false;
        }
        if (entry.requiresApiKey === false) {
            return true;
        }
        try {
            return entry.backend.hasApiKey();
        } catch (error) {
            return false;
        }
    }

    function endpointLabel(entry) {
        if (!entry || !entry.backend) {
            return i18n("Unavailable");
        }
        if (entry.backend.customBaseUrl && entry.backend.customBaseUrl.length > 0) {
            return entry.backend.customBaseUrl;
        }
        return i18n("Default provider endpoint");
    }

    function costSourceLabel(entry) {
        if (!entry || !entry.backend) {
            return i18n("Unknown");
        }
        return entry.backend.isEstimatedCost
            ? i18n("Estimated from pricing tables")
            : i18n("Provider-reported billing");
    }

    function anomalySummary() {
        var anomalies = overview.anomalies || [];
        if (anomalies.length === 0) {
            return i18n("No anomalies");
        }
        var first = anomalies[0];
        return i18n("%1 on %2", formatCurrency(first.value), formatDateLabel(first.date));
    }

    function buildReport(days) {
        if (!db) {
            return i18n("History is disabled. Enable history to generate Analyst reports.");
        }

        var reportOverview = db.getAnalystOverview(days) || ({});
        var reportEfficiency = db.getEfficiencySeries(days) || [];
        var lines = [];
        lines.push(i18n("AI Usage Monitor Analyst Report (%1 days)", days));
        lines.push(i18n("Generated: %1", new Date().toLocaleString()));
        lines.push("");
        lines.push(i18n("Current daily spend: %1", formatCurrency(reportOverview.currentDailyCost)));
        lines.push(i18n("Average daily spend: %1", formatCurrency(reportOverview.averageDailyCost)));
        lines.push(i18n("Week-over-week trend: %1", formatPercent(reportOverview.weekOverWeekPercent)));
        lines.push(i18n("Volatility: %1", formatPercent(reportOverview.volatilityPercent)));

        if (reportEfficiency.length > 0) {
            var total = 0;
            for (var i = 0; i < reportEfficiency.length; ++i) {
                total += Number(reportEfficiency[i].value);
            }
            lines.push(i18n("Average output / input ratio: %1x", (total / reportEfficiency.length).toFixed(2)));
        }

        lines.push("");
        lines.push(i18n("Top spend drivers:"));
        var drivers = reportOverview.topDrivers || [];
        if (drivers.length === 0) {
            lines.push(i18n("- No cost drivers available yet"));
        } else {
            for (var j = 0; j < Math.min(3, drivers.length); ++j) {
                var driver = drivers[j];
                lines.push(i18n("- %1 (%2): %3%4",
                                driver.provider,
                                driver.model,
                                formatCurrency(driver.value),
                                driver.estimated ? i18n(" estimated") : ""));
            }
        }

        lines.push("");
        lines.push(i18n("Detected anomalies:"));
        var anomalies = reportOverview.anomalies || [];
        if (anomalies.length === 0) {
            lines.push(i18n("- None in the selected window"));
        } else {
            for (var k = 0; k < Math.min(3, anomalies.length); ++k) {
                var anomaly = anomalies[k];
                lines.push(i18n("- %1 at %2 (%3)",
                                formatDateLabel(anomaly.date),
                                formatCurrency(anomaly.value),
                                formatPercent(anomaly.deltaPercent)));
            }
        }

        return lines.join("\n");
    }

    function copyDiagnostics() {
        var entry = selectedProviderEntry();
        if (!entry || !entry.backend) {
            return;
        }

        var lines = [];
        lines.push(i18n("Provider Diagnostics: %1", entry.name));
        lines.push(i18n("Connected: %1", entry.backend.connected ? i18n("Yes") : i18n("No")));
        lines.push(i18n("Auth configured: %1", authConfigured(entry) ? i18n("Yes") : i18n("No")));
        lines.push(i18n("Cost source: %1", costSourceLabel(entry)));
        lines.push(i18n("Endpoint: %1", endpointLabel(entry)));
        lines.push(i18n("Last refresh: %1", relativeRefresh(entry)));
        lines.push(i18n("Error count: %1", entry.backend.errorCount || 0));
        if (entry.backend.error && entry.backend.error.length > 0) {
            lines.push(i18n("Last error: %1", entry.backend.error));
        }
        clipboard.setText(lines.join("\n"));
    }

    Component.onCompleted: refreshData()
    onVisibleChanged: if (visible) refreshData()

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18n("Refresh")
            onTriggered: refreshData()
        }
    ]

    AIUsage.ClipboardHelper {
        id: clipboard
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            EfficiencyMetricCard {
                Layout.fillWidth: true
                visible: analystPage.hasEfficiencyData
                efficiencyRatio: avgEfficiency
            }

            Kirigami.Card {
                Layout.fillWidth: true

                header: Kirigami.Heading {
                    text: i18n("Week-over-Week Spend")
                    level: 3
                }

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Label {
                        text: formatPercent(overview.weekOverWeekPercent)
                        font.pointSize: 22
                        font.weight: Font.Bold
                        color: Number(overview.weekOverWeekPercent || 0) > 0
                            ? Kirigami.Theme.negativeTextColor
                            : (Number(overview.weekOverWeekPercent || 0) < 0
                               ? Kirigami.Theme.positiveTextColor
                               : Kirigami.Theme.disabledTextColor)
                    }

                    Controls.Label {
                        text: i18n("Average daily spend: %1", formatCurrency(overview.averageDailyCost))
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }

            Kirigami.Card {
                Layout.fillWidth: true

                header: Kirigami.Heading {
                    text: i18n("Volatility")
                    level: 3
                }

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Label {
                        text: formatPercent(overview.volatilityPercent)
                        font.pointSize: 22
                        font.weight: Font.Bold
                    }

                    Controls.Label {
                        text: i18n("%1 anomaly days in window", overview.anomalyCount || 0)
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Controls.Label {
                        text: anomalySummary()
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true

            header: RowLayout {
                Layout.fillWidth: true
                Kirigami.Heading {
                    text: i18n("Activity Heatmap (Last 365 Days)")
                    level: 3
                    Layout.fillWidth: true
                }
                Controls.ComboBox {
                    model: [i18n("Cost Intensity"), i18n("Volume Intensity")]
                    currentIndex: plasmoid.configuration.analystIntensityMode
                    onActivated: function(index) {
                        plasmoid.configuration.analystIntensityMode = index;
                        refreshData();
                    }
                }
                Controls.CheckBox {
                    text: i18n("Normalize Outliers")
                    checked: plasmoid.configuration.analystNormalization
                    onToggled: {
                        plasmoid.configuration.analystNormalization = checked;
                        refreshData();
                    }
                }
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                Controls.ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    contentWidth: heatmap.width
                    clip: true

                    ActivityHeatmap {
                        id: heatmap
                        activityData: analystPage.activityData
                        maxIntensity: analystPage.maxIntensity
                        baseColor: Kirigami.Theme.highlightColor

                        onHovered: function(date, value, recorded) {
                            hoverLabel.text = recorded
                                ? i18n("%1: %2", date,
                                       plasmoid.configuration.analystIntensityMode === 0
                                           ? formatCurrency(value)
                                           : Utils.formatNumber(value) + i18n(" tokens"))
                                : i18n("%1: No recorded data", date);
                        }
                    }
                }

                Controls.Label {
                    id: hoverLabel
                    Layout.alignment: Qt.AlignHCenter
                    text: i18n("Hover over a day to inspect activity")
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Card {
                Layout.fillWidth: true

                header: Kirigami.Heading {
                    text: i18n("Top Cost Drivers")
                    level: 3
                }

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: overview.topDrivers || []

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Controls.Label {
                                text: (index + 1) + "."
                                color: Kirigami.Theme.disabledTextColor
                            }

                            Controls.Label {
                                Layout.fillWidth: true
                                text: modelData.provider + "  [" + modelData.model + "]"
                                elide: Text.ElideRight
                            }

                            Controls.Label {
                                text: formatCurrency(modelData.value)
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Controls.Label {
                        visible: (overview.topDrivers || []).length === 0
                        text: i18n("No spend drivers recorded yet.")
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }

            Kirigami.Card {
                Layout.fillWidth: true

                header: Kirigami.Heading {
                    text: i18n("Model Exposure")
                    level: 3
                }

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: overview.topModels || []

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Controls.Label {
                                Layout.fillWidth: true
                                text: modelData.model
                                elide: Text.ElideRight
                            }

                            Controls.Label {
                                text: formatCurrency(modelData.value)
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Controls.Label {
                        visible: (overview.topModels || []).length === 0
                        text: i18n("Model metadata will appear after providers refresh.")
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true

            header: Kirigami.Heading {
                text: i18n("Provider Diagnostics")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    Controls.ComboBox {
                        id: diagnosticsProviderCombo
                        Layout.fillWidth: true
                        model: enabledProviderEntries()
                        textRole: "label"
                        onCurrentIndexChanged: {}
                    }

                    Controls.Button {
                        text: i18n("Copy")
                        icon.name: "edit-copy"
                        enabled: enabledProviderEntries().length > 0
                        onClicked: copyDiagnostics()
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Controls.Label { text: i18n("Auth") }
                    Controls.Label {
                        text: authConfigured(selectedProviderEntry()) ? i18n("Configured") : i18n("Missing")
                        color: authConfigured(selectedProviderEntry())
                            ? Kirigami.Theme.positiveTextColor
                            : Kirigami.Theme.negativeTextColor
                    }

                    Controls.Label { text: i18n("Connection") }
                    Controls.Label {
                        text: selectedProviderEntry() && selectedProviderEntry().backend && selectedProviderEntry().backend.connected
                            ? i18n("Healthy")
                            : i18n("Disconnected")
                        color: selectedProviderEntry() && selectedProviderEntry().backend && selectedProviderEntry().backend.connected
                            ? Kirigami.Theme.positiveTextColor
                            : Kirigami.Theme.negativeTextColor
                    }

                    Controls.Label { text: i18n("Last refresh") }
                    Controls.Label { text: relativeRefresh(selectedProviderEntry()) }

                    Controls.Label { text: i18n("Cost source") }
                    Controls.Label {
                        text: costSourceLabel(selectedProviderEntry())
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Controls.Label { text: i18n("Endpoint") }
                    Controls.Label {
                        text: endpointLabel(selectedProviderEntry())
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Controls.Label { text: i18n("Requests remaining") }
                    Controls.Label {
                        text: selectedProviderEntry() && selectedProviderEntry().backend
                            ? Number(selectedProviderEntry().backend.rateLimitRequestsRemaining || 0).toString()
                            : i18n("N/A")
                    }
                }

                Controls.Label {
                    visible: selectedProviderEntry() && selectedProviderEntry().backend && selectedProviderEntry().backend.error
                    text: selectedProviderEntry() && selectedProviderEntry().backend
                        ? i18n("Last error: %1", selectedProviderEntry().backend.error)
                        : ""
                    color: Kirigami.Theme.negativeTextColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true

            header: Kirigami.Heading {
                text: i18n("Anomalies")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: overview.anomalies || []

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            Layout.fillWidth: true
                            text: formatDateLabel(modelData.date)
                        }

                        Controls.Label {
                            text: formatCurrency(modelData.value)
                            font.weight: Font.DemiBold
                        }

                        Controls.Label {
                            text: formatPercent(modelData.deltaPercent)
                            color: Kirigami.Theme.negativeTextColor
                        }
                    }
                }

                Controls.Label {
                    visible: (overview.anomalies || []).length === 0
                    text: i18n("No anomalous daily spend spikes detected in the active window.")
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: intelligenceEngine && intelligenceEngine.enabled

            header: Kirigami.Heading {
                text: i18n("Insights & Reports")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                Controls.Label {
                    text: intelligenceEngine ? intelligenceEngine.lastAnalystInsight : i18n("No insights available.")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Button {
                        text: intelligenceEngine && intelligenceEngine.busy ? i18n("Generating...") : i18n("Generate Insight")
                        icon.name: "view-refresh"
                        enabled: !(intelligenceEngine && intelligenceEngine.busy)
                        onClicked: generateAnalystInsight()
                    }

                    Controls.Button {
                        text: i18n("Copy Weekly Report")
                        icon.name: "edit-copy"
                        onClicked: clipboard.setText(buildReport(7))
                    }

                    Controls.Button {
                        text: i18n("Copy Monthly Report")
                        icon.name: "edit-copy"
                        onClicked: clipboard.setText(buildReport(30))
                    }
                }
            }
        }
    }
}
