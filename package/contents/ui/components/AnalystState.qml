import QtQuick

QtObject {
    id: state

    property var db: null
    property var snapshot: ({})
    property bool loading: false
    property string displayRequestId: ""
    property string reportRequestId: ""
    property int reportDays: 0
    property int requestGeneration: 0

    readonly property var coverage: snapshot.coverage || ({})
    readonly property bool hasSnapshot: snapshot.ok === true

    property MetricAvailabilityFormatter formatter: MetricAvailabilityFormatter {}

    signal reportReady(string report)

    property Connections databaseConnections: Connections {
        target: state.db
        enabled: state.db !== null
        ignoreUnknownSignals: true

        function onAnalystReady(requestId, result) {
            state.acceptResult(requestId, result);
        }
    }

    function exactRange(days, nowValue) {
        var now = nowValue ? new Date(nowValue) : new Date();
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

    function acceptResult(requestId, result) {
        if (requestId === displayRequestId) {
            snapshot = result || ({});
            loading = false;
        } else if (requestId === reportRequestId) {
            reportReady(buildReport(result || ({}), reportDays));
            reportRequestId = "";
        }
    }

    function kpi(name) {
        return formatter.kpi(snapshot, name);
    }

    function reasonText(reasonKey, sampleCount, minimumSamples) {
        return formatter.reasonText(reasonKey, sampleCount, minimumSamples);
    }

    function formatMoney(value, currency) {
        return formatter.formatMoney(value, currency);
    }

    function formatPercent(value) {
        return formatter.formatPercent(value);
    }

    function formatDate(value) {
        return formatter.formatDate(value);
    }

    function currencyStatusText(status, currency) {
        return formatter.currencyStatusText(status, currency);
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
            {
                name: i18n("Actual"),
                color: "#10A37F",
                currency: snapshot.currency || "",
                points: actual
            },
            {
                name: i18n("Estimated"),
                color: "#D4A574",
                currency: snapshot.currency || "",
                points: estimated
            }
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

        function appendKpi(label, key, formatterFunction) {
            var item = reportKpis[key] || ({ available: false });
            if (item.available) {
                lines.push(label + ": " + formatterFunction(item.value));
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
}
