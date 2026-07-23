import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "Utils.js" as Utils

Kirigami.ScrollablePage {
    id: analystPage

    title: i18n("The Analyst")

    property var snapshot: ({})
    property bool loading: false
    property string displayRequestId: ""
    property string reportRequestId: ""
    property int reportDays: 0
    property int requestGeneration: 0

    readonly property var db: plasmoid.configuration.historyEnabled
        ? root.usageDb
        : null
    readonly property var coverage: snapshot.coverage || ({})
    readonly property var kpis: snapshot.kpis || ({})
    readonly property bool hasSnapshot: snapshot.ok === true

    function exactRange(days) {
        var now = new Date();
        var end = new Date(now.getFullYear(), now.getMonth(), now.getDate(),
                           23, 59, 59, 999);
        var start = new Date(end);
        start.setDate(end.getDate() - days + 1);
        start.setHours(0, 0, 0, 0);
        return { from: start, to: end };
    }

    function nextRequestId(prefix) {
        requestGeneration += 1;
        return prefix + "-" + requestGeneration;
    }

    function refreshData() {
        if (!db || typeof db.requestAnalyst !== "function") {
            loading = false;
            snapshot = ({ ok: false, errorKey: "history_unavailable" });
            return;
        }
        var range = exactRange(30);
        displayRequestId = nextRequestId("analyst-display");
        loading = true;
        db.requestAnalyst(displayRequestId, range.from, range.to, "");
    }

    function requestReport(days) {
        if (!db || typeof db.requestAnalyst !== "function") {
            return;
        }
        var range = exactRange(days);
        reportDays = days;
        reportRequestId = nextRequestId("analyst-report-" + days);
        db.requestAnalyst(reportRequestId, range.from, range.to, "");
    }

    function kpi(name) {
        return kpis[name] || ({
            available: false,
            reasonKey: "unavailable",
            sampleCount: 0,
            minimumSamples: 0
        });
    }

    function reasonText(reasonKey, sampleCount, minimumSamples) {
        switch (reasonKey) {
        case "mixed_currencies":
            return i18n("Cost analysis is paused because the period contains multiple currencies.");
        case "no_compatible_cost":
            return i18n("No compatible interval spend is recorded for this period.");
        case "insufficient_daily_samples":
            return i18n("At least %1 recorded days are required; %2 are available.",
                        minimumSamples, sampleCount);
        case "incomplete_comparison_windows":
            return i18n("Week-over-week change requires two complete seven-day windows.");
        case "zero_previous_window":
            return i18n("The previous seven-day window is zero, so a percentage change is unavailable.");
        case "zero_baseline":
            return i18n("The recorded baseline is zero, so relative volatility is unavailable.");
        case "insufficient_ratio_samples":
            return i18n("At least %1 days with positive input tokens are required; %2 are available.",
                        minimumSamples, sampleCount);
        case "no_compatible_activity":
            return i18n("No compatible token, request, or local-tool activity is recorded for this period.");
        case "history_unavailable":
            return i18n("History is disabled or the native history service is unavailable.");
        default:
            return i18n("This result is unavailable for the selected period.");
        }
    }

    function formatMoney(value, currency) {
        return Utils.formatMoney(Number(value), currency || "");
    }

    function formatPercent(value) {
        var numeric = Number(value);
        var prefix = numeric > 0 ? "+" : "";
        return prefix + numeric.toFixed(1) + "%";
    }

    function formatDate(value) {
        if (!value) {
            return i18n("Unknown");
        }
        return new Date(value).toLocaleDateString();
    }

    function currencyStatusText(status, currency) {
        switch (status) {
        case "single":
            return i18n("Single currency (%1)", currency);
        case "selected":
            return i18n("Selected currency (%1)", currency);
        case "mixed":
            return i18n("Mixed currencies");
        default:
            return i18n("No compatible cost currency");
        }
    }

    function averageSpendLabel(data) {
        var actual = Number(data.actualSampleCount || 0);
        var estimated = Number(data.estimatedSampleCount || 0);
        if (actual > 0 && estimated > 0) {
            return i18n("Average daily spend (actual + estimated)");
        }
        if (estimated > 0) {
            return i18n("Average daily spend (estimated)");
        }
        return i18n("Average daily spend (actual)");
    }

    function spendChartSeries() {
        var actual = [];
        var estimated = [];
        var rows = snapshot.spendSeries || [];
        for (var i = 0; i < rows.length; ++i) {
            actual.push({
                timestamp: rows[i].date + "T12:00:00Z",
                available: rows[i].actualAvailable,
                value: rows[i].actual
            });
            estimated.push({
                timestamp: rows[i].date + "T12:00:00Z",
                available: rows[i].estimatedAvailable,
                value: rows[i].estimated
            });
        }
        return [
            { name: i18n("Actual"), color: "#10A37F", points: actual },
            { name: i18n("Estimated"), color: "#D4A574", points: estimated }
        ];
    }

    function activityChartSeries(kind) {
        var points = [];
        var rows = snapshot.activitySeries || [];
        var availableKey = kind + "Available";
        for (var i = 0; i < rows.length; ++i) {
            points.push({
                timestamp: rows[i].date + "T12:00:00Z",
                available: rows[i][availableKey] === true,
                value: rows[i][kind]
            });
        }
        return [{
            name: kind === "tokens" ? i18n("Tokens")
                  : (kind === "requests" ? i18n("Requests")
                                         : i18n("Local tool usage")),
            color: kind === "tokens" ? "#4285F4"
                  : (kind === "requests" ? "#6e40c9" : "#FF7000"),
            points: points
        }];
    }

    function seriesHasValues(kind) {
        var rows = snapshot.activitySeries || [];
        var availableKey = kind + "Available";
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i][availableKey] === true) {
                return true;
            }
        }
        return false;
    }

    function buildReport(reportSnapshot, days) {
        var reportKpis = reportSnapshot.kpis || ({});
        var reportCoverage = reportSnapshot.coverage || ({});
        var currency = reportSnapshot.currency || "";
        var lines = [];
        lines.push(i18n("AI Usage Monitor Analyst Report (%1 days)", days));
        lines.push(i18n("Generated: %1", new Date(reportSnapshot.generatedAt).toLocaleString()));
        lines.push(i18n("Period: %1 to %2",
                        formatDate(reportSnapshot.from),
                        formatDate(reportSnapshot.to)));
        lines.push(i18n("Coverage: %1 of %2 days (%3%)",
                        reportCoverage.observedDayCount || 0,
                        reportCoverage.requestedDayCount || days,
                        Number(reportCoverage.percent || 0).toFixed(0)));
        lines.push(i18n("Cost currency status: %1",
                        currencyStatusText(reportSnapshot.currencyStatus,
                                           currency)));
        lines.push(i18n("Cost samples: %1 actual, %2 estimated",
                        reportSnapshot.actualSampleCount || 0,
                        reportSnapshot.estimatedSampleCount || 0));
        lines.push("");

        function appendKpi(label, key, formatter) {
            var item = reportKpis[key] || ({ available: false });
            if (item.available) {
                lines.push(label + ": " + formatter(item.value));
            } else {
                lines.push(label + ": " + i18n("Unavailable — %1",
                    reasonText(item.reasonKey, item.sampleCount,
                               item.minimumSamples)));
            }
        }

        appendKpi(averageSpendLabel(reportSnapshot), "averageDailySpend",
                  function(value) { return formatMoney(value, currency); });
        appendKpi(i18n("Week-over-week change"), "weekOverWeekChange",
                  formatPercent);
        appendKpi(i18n("Volatility"), "volatility", formatPercent);
        appendKpi(i18n("Average output / input ratio"), "outputInputRatio",
                  function(value) { return Number(value).toFixed(2) + "x"; });

        lines.push("");
        lines.push(i18n("Top compatible spend drivers:"));
        var drivers = reportSnapshot.topDrivers || [];
        if (drivers.length === 0) {
            lines.push(i18n("- Unavailable for this period"));
        } else {
            for (var i = 0; i < Math.min(5, drivers.length); ++i) {
                var driver = drivers[i];
                lines.push(i18n("- %1 (%2, %3): %4",
                                driver.provider, driver.model,
                                driver.quality,
                                formatMoney(driver.value, driver.currency)));
            }
        }

        lines.push("");
        lines.push(i18n("Anomaly candidates:"));
        if (reportSnapshot.anomaliesAvailable !== true) {
            lines.push(i18n("- Unavailable — %1",
                            reasonText(reportSnapshot.anomaliesReasonKey,
                                       (reportSnapshot.spendSeries || []).length,
                                       7)));
        } else if ((reportSnapshot.anomalies || []).length === 0) {
            lines.push(i18n("- None crossed the documented threshold"));
        } else {
            var anomalies = reportSnapshot.anomalies || [];
            for (var j = 0; j < Math.min(5, anomalies.length); ++j) {
                lines.push(i18n("- %1: %2 (period baseline %3)",
                                anomalies[j].date,
                                formatMoney(anomalies[j].value,
                                            anomalies[j].currency),
                                formatMoney(anomalies[j].baseline,
                                            anomalies[j].currency)));
            }
        }
        lines.push("");
        lines.push(i18n("Method: volatility requires seven recorded days; anomaly candidates require a value at least two standard deviations above the period mean and a material absolute increase."));
        lines.push(i18n("Output / input ratio is descriptive and does not measure quality, productivity, or prompt clarity."));
        return lines.join("\n");
    }

    function writtenSummary() {
        if (!hasSnapshot) {
            return i18n("No Analyst snapshot is available.");
        }
        var parts = [];
        var average = kpi("averageDailySpend");
        if (average.available) {
            parts.push(i18n("%1 is %2.",
                            averageSpendLabel(snapshot),
                            formatMoney(average.value, snapshot.currency)));
        } else {
            parts.push(reasonText(average.reasonKey, average.sampleCount,
                                  average.minimumSamples));
        }
        if (snapshot.activityAvailable === true) {
            parts.push(i18n("Compatible activity is available independently of cost analysis."));
        }
        if (snapshot.anomaliesAvailable === true) {
            parts.push((snapshot.anomalies || []).length > 0
                ? i18np("%1 anomaly candidate crossed the documented threshold.",
                        "%1 anomaly candidates crossed the documented threshold.",
                        snapshot.anomalies.length)
                : i18n("No day crossed the documented anomaly threshold."));
        }
        return parts.join(" ");
    }

    Component.onCompleted: refreshData()
    onVisibleChanged: if (visible && reportRequestId === "") refreshData()

    Connections {
        target: analystPage.db
        enabled: analystPage.db !== null

        function onAnalystReady(requestId, result) {
            if (requestId === analystPage.displayRequestId) {
                analystPage.snapshot = result || ({})
                analystPage.loading = false
            } else if (requestId === analystPage.reportRequestId) {
                clipboard.setText(analystPage.buildReport(result || ({}),
                                                          analystPage.reportDays))
                analystPage.reportRequestId = ""
            }
        }
    }

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18n("Refresh")
            enabled: analystPage.reportRequestId === ""
            onTriggered: analystPage.refreshData()
        },
        Kirigami.Action {
            icon.name: "edit-copy"
            text: i18n("Copy 7-day report")
            enabled: analystPage.db !== null
                && !analystPage.loading
                && analystPage.reportRequestId === ""
            onTriggered: analystPage.requestReport(7)
        },
        Kirigami.Action {
            icon.name: "edit-copy"
            text: i18n("Copy 30-day report")
            enabled: analystPage.db !== null
                && !analystPage.loading
                && analystPage.reportRequestId === ""
            onTriggered: analystPage.requestReport(30)
        }
    ]

    ClipboardHelper {
        id: clipboard
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: analystPage.loading
            running: visible
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !analystPage.loading && !analystPage.hasSnapshot
            type: Kirigami.MessageType.Information
            text: reasonText(analystPage.snapshot.errorKey || "history_unavailable",
                             0, 0)
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot

            header: Kirigami.Heading {
                text: i18n("Data coverage")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    objectName: "coverageLabel"
                    text: i18n("%1 of %2 days contain compatible observations (%3%)",
                               analystPage.coverage.observedDayCount || 0,
                               analystPage.coverage.requestedDayCount || 30,
                               Number(analystPage.coverage.percent || 0).toFixed(0))
                    wrapMode: Text.WordWrap
                }

                Controls.Label {
                    text: i18n("Period: %1 – %2",
                               formatDate(analystPage.snapshot.from),
                               formatDate(analystPage.snapshot.to))
                    color: Kirigami.Theme.disabledTextColor
                }

                Controls.Label {
                    text: i18n("%1 actual and %2 estimated compatible cost samples",
                               analystPage.snapshot.actualSampleCount || 0,
                               analystPage.snapshot.estimatedSampleCount || 0)
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot
                && !analystPage.kpi("averageDailySpend").available
            type: analystPage.snapshot.mixedCurrencies
                ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Information
            text: analystPage.reasonText(
                analystPage.kpi("averageDailySpend").reasonKey,
                analystPage.kpi("averageDailySpend").sampleCount,
                analystPage.kpi("averageDailySpend").minimumSamples)
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot
                && analystPage.kpi("averageDailySpend").available

            header: Kirigami.Heading {
                text: i18n("Spend trend")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= Kirigami.Units.gridUnit * 36 ? 3 : 1
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    ColumnLayout {
                        Controls.Label {
                            text: analystPage.averageSpendLabel(
                                analystPage.snapshot)
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            objectName: "averageSpendValue"
                            text: formatMoney(
                                analystPage.kpi("averageDailySpend").value,
                                analystPage.snapshot.currency)
                            font.pointSize: 20
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        Controls.Label {
                            text: i18n("Week over week")
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            text: analystPage.kpi("weekOverWeekChange").available
                                ? formatPercent(analystPage.kpi("weekOverWeekChange").value)
                                : i18n("Unavailable")
                            font.pointSize: 20
                            font.weight: Font.Bold
                        }
                        Controls.Label {
                            visible: !analystPage.kpi("weekOverWeekChange").available
                            text: analystPage.reasonText(
                                analystPage.kpi("weekOverWeekChange").reasonKey,
                                analystPage.kpi("weekOverWeekChange").sampleCount,
                                analystPage.kpi("weekOverWeekChange").minimumSamples)
                            wrapMode: Text.WordWrap
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    ColumnLayout {
                        Controls.Label {
                            text: i18n("Volatility")
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            text: analystPage.kpi("volatility").available
                                ? formatPercent(analystPage.kpi("volatility").value)
                                : i18n("Unavailable")
                            font.pointSize: 20
                            font.weight: Font.Bold
                        }
                        Controls.Label {
                            visible: !analystPage.kpi("volatility").available
                            text: analystPage.reasonText(
                                analystPage.kpi("volatility").reasonKey,
                                analystPage.kpi("volatility").sampleCount,
                                analystPage.kpi("volatility").minimumSamples)
                            wrapMode: Text.WordWrap
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }

                MultiSeriesChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 12
                    metric: "cost"
                    seriesData: analystPage.spendChartSeries()
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot
                && analystPage.snapshot.activityAvailable === true

            header: Kirigami.Heading {
                text: i18n("Activity trend")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                MultiSeriesChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                    visible: analystPage.seriesHasValues("tokens")
                    metric: "tokens"
                    seriesData: analystPage.activityChartSeries("tokens")
                }

                MultiSeriesChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                    visible: analystPage.seriesHasValues("requests")
                    metric: "requests"
                    seriesData: analystPage.activityChartSeries("requests")
                }

                ColumnLayout {
                    visible: analystPage.seriesHasValues("toolUsage")
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        text: i18n("Local tool activity")
                        level: 4
                    }

                    MultiSeriesChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                        metric: "requests"
                        seriesData: analystPage.activityChartSeries("toolUsage")
                    }
                }

                EfficiencyMetricCard {
                    Layout.fillWidth: true
                    visible: analystPage.kpi("outputInputRatio").available
                    efficiencyRatio: Number(
                        analystPage.kpi("outputInputRatio").value)
                }

                Controls.Label {
                    visible: !analystPage.kpi("outputInputRatio").available
                    text: analystPage.reasonText(
                        analystPage.kpi("outputInputRatio").reasonKey,
                        analystPage.kpi("outputInputRatio").sampleCount,
                        analystPage.kpi("outputInputRatio").minimumSamples)
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot
                && (analystPage.snapshot.topDrivers || []).length > 0

            header: Kirigami.Heading {
                text: i18n("Top compatible spend drivers")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: analystPage.snapshot.topDrivers || []

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            text: (index + 1) + "."
                            color: Kirigami.Theme.disabledTextColor
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Controls.Label {
                                Layout.fillWidth: true
                                text: modelData.provider + "  [" + modelData.model + "]"
                                elide: Text.ElideRight
                            }
                            Controls.Label {
                                text: modelData.quality === "estimated"
                                    ? i18n("Estimated")
                                    : i18n("Actual")
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        Controls.Label {
                            text: formatMoney(modelData.value,
                                              modelData.currency)
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot
                && analystPage.snapshot.anomaliesAvailable === true

            header: Kirigami.Heading {
                text: i18n("Anomaly candidates")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    visible: (analystPage.snapshot.anomalies || []).length === 0
                    text: i18n("No day crossed the documented threshold.")
                    color: Kirigami.Theme.disabledTextColor
                }

                Repeater {
                    model: analystPage.snapshot.anomalies || []

                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18n("%1: %2, compared with a %3 period baseline",
                                   modelData.date,
                                   formatMoney(modelData.value,
                                               modelData.currency),
                                   formatMoney(modelData.baseline,
                                               modelData.currency))
                        wrapMode: Text.WordWrap
                    }
                }

                Controls.Label {
                    text: i18n("Candidates require at least seven recorded days, two standard deviations above the period mean, and a material absolute increase.")
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystPage.hasSnapshot

            header: Kirigami.Heading {
                text: i18n("Period summary")
                level: 3
            }

            contentItem: Controls.Label {
                objectName: "writtenSummary"
                text: analystPage.writtenSummary()
                wrapMode: Text.WordWrap
            }
        }
    }
}
