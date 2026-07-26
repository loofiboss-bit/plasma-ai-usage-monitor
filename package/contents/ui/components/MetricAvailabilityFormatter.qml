import QtQuick
import "../Utils.js" as Utils

QtObject {
    function unavailableValue() {
        return "\u2014";
    }

    function unavailableLabel() {
        return i18n("Unavailable");
    }

    function kpi(snapshot, name) {
        var values = snapshot && snapshot.kpis ? snapshot.kpis : {};
        return values[name] || {
            available: false,
            reasonKey: "unavailable",
            sampleCount: 0,
            minimumSamples: 0
        };
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
}
