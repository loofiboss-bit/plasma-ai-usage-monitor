import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

/**
 * Lightweight deterministic analyst insight generator.
 * Keeps v5 useful without requiring a local LLM service.
 */
Item {
    id: engine
    
    property bool enabled: true
    property string lastAnalystInsight: i18n("Welcome to The Analyst. Click 'Generate' to analyze your usage patterns.")
    property bool busy: false
    property var pendingOverview: ({})
    property var pendingEfficiency: []

    function generateInsight(heatmapData, efficiencyData, overview) {
        pendingOverview = overview || {};
        pendingEfficiency = efficiencyData || [];
        busy = true;
        insightTimer.start();
    }

    Timer {
        id: insightTimer
        interval: 450
        repeat: false
        onTriggered: {
            engine.busy = false;
            var overview = engine.pendingOverview || {};
            var drivers = overview.topDrivers || [];
            var anomalies = overview.anomalies || [];
            var efficiency = engine.pendingEfficiency || [];

            var averageCost = overview.averageDailyCost || 0;
            var wow = overview.weekOverWeekPercent || 0;
            var volatility = overview.volatilityPercent || 0;
            var efficiencyAvg = 0;
            if (efficiency.length > 0) {
                var sum = 0;
                for (var i = 0; i < efficiency.length; ++i) {
                    sum += efficiency[i].value || 0;
                }
                efficiencyAvg = sum / efficiency.length;
            }

            var lines = [];
            if (overview.mixedCurrencies) {
                lines.push(i18n("Spend insights are paused because the selected history contains mixed currencies and no conversion source is configured."));
                return lines;
            }
            var currency = overview.currency || "USD";
            if (drivers.length > 0) {
                var lead = drivers[0];
                lines.push(i18n("%1 is currently the top spend driver at about %2 for the month.",
                                lead.provider, Utils.formatMoney(lead.value || 0, currency)));
            } else {
                lines.push(i18n("Not enough cost history is available yet to identify a spend driver."));
            }

            if (anomalies.length > 0) {
                var anomaly = anomalies[0];
                lines.push(i18n("A spending spike was detected on %1 at %2, around %3% above baseline.",
                                anomaly.date, Utils.formatMoney(anomaly.value || 0, currency),
                                Math.round(anomaly.deltaPercent || 0)));
            } else if (averageCost > 0) {
                lines.push(i18n("Daily spending is stable around %1 with no major anomalies in the current window.",
                                Utils.formatMoney(averageCost, currency)));
            }

            if (wow > 5) {
                lines.push(i18n("Week-over-week spend is trending up by %1%. Consider checking model choice or refresh cadence.",
                                wow.toFixed(1)));
            } else if (wow < -5) {
                lines.push(i18n("Week-over-week spend is down by %1%, which suggests tighter usage or cheaper model mix.",
                                Math.abs(wow).toFixed(1)));
            } else {
                lines.push(i18n("Week-over-week spend is broadly flat, which usually means usage is staying within its recent band."));
            }

            if (efficiencyAvg > 1.5) {
                lines.push(i18n("Prompt efficiency is strong at %1x output/input, indicating concise prompts with healthy completion volume.",
                                efficiencyAvg.toFixed(2)));
            } else if (efficiencyAvg > 0) {
                lines.push(i18n("Prompt efficiency is %1x output/input. There is room to tighten prompts if costs rise faster than value.",
                                efficiencyAvg.toFixed(2)));
            }

            if (volatility > 40) {
                lines.push(i18n("Cost volatility is elevated at %1%, so short-term spikes are worth monitoring closely.",
                                volatility.toFixed(1)));
            }

            engine.lastAnalystInsight = lines.join(" ");
        }
    }
}
