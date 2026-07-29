import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "RunwayUi"

    function i18n(message) {
        var result = message;
        for (var i = 1; i < arguments.length; i++)
            result = result.replace("%" + i, arguments[i]);
        return result;
    }

    function i18np(singular, plural, count) {
        var args = [count === 1 ? singular : plural];
        for (var i = 2; i < arguments.length; i++) args.push(arguments[i]);
        if (args.length === 1) args.push(count);
        return i18n.apply(null, args);
    }

    QtObject {
        id: presentation
        property var topAction: ({})
        function headline() { return "Current source state"; }
        function explanation() { return "Current state explanation"; }
        function focusFacts() { return []; }
        function actionLabel(row) { return row.stableId ? "Fix current issue" : ""; }
        function actionIcon(row) { return "configure"; }
    }

    QtObject {
        id: guardrails
        property var forecasts: []
    }

    Component {
        id: runwayCardComponent
        Monitor.RunwayCard {
            width: 640
            forecast: ({})
        }
    }

    Component {
        id: dailyFocusComponent
        Components.DailyFocus {
            width: 640
        }
    }

    function forecast(state, options) {
        var values = options || {};
        return {
            kind: values.kind || "quota_exhaustion",
            state: state,
            sourceId: values.sourceId || "openai",
            sourceKind: "provider",
            window: "requests_minute",
            scope: "api_key",
            currentValue: values.currentValue === undefined ? 40 : values.currentValue,
            projectedValue: values.projectedValue === undefined ? 0 : values.projectedValue,
            limitValue: values.limitValue === undefined ? 100 : values.limitValue,
            unit: "request",
            currency: null,
            predictedAt: state === "warning" || state === "critical"
                ? new Date(Date.now() + 60 * 60 * 1000) : null,
            periodEnd: new Date(Date.now() + 2 * 60 * 60 * 1000),
            sampleCount: values.sampleCount || 8,
            coveragePercent: values.coveragePercent || 90,
            evidenceGrade: state === "unavailable" ? "unavailable" : "strong",
            methodId: "quota-runway-v1",
            reasonKey: state === "unavailable" ? "missing_value" : "",
            reasonText: state === "unavailable"
                ? "A required value is unavailable" : "",
            valueClass: "actual"
        };
    }

    function init() {
        presentation.topAction = ({});
        guardrails.forecasts = [];
    }

    function test_missingAndNumericZeroStayDistinct() {
        var unavailable = forecast("unavailable", {
            currentValue: null,
            projectedValue: null,
            limitValue: null
        });
        var card = createTemporaryObject(
            runwayCardComponent, testCase, { forecast: unavailable });
        verify(card);
        compare(card.stateLabel(), "Unavailable");
        compare(card.formatValue(unavailable.currentValue), "—");
        compare(card.summaryText(), "A required value is unavailable");

        var zero = forecast("critical", { currentValue: 0 });
        card.forecast = zero;
        verify(card.formatValue(zero.currentValue).indexOf("0") >= 0);
        verify(card.formatValue(zero.currentValue) !== "—");
    }

    function test_currentIncidentOutranksForecast() {
        presentation.topAction = {
            stableId: "anthropic",
            displayName: "Anthropic",
            sourceKind: "provider",
            nextActionKey: "verify_source",
            attentionSeverity: "critical"
        };
        guardrails.forecasts = [forecast("critical")];
        var focus = createTemporaryObject(dailyFocusComponent, testCase, {
            presentation: presentation,
            guardrails: guardrails
        });
        verify(focus);
        compare(focus.showingRunway, false);
        compare(focus.headlineText(), "Current source state");
        compare(focus.effectiveActionRow.stableId, "anthropic");
    }

    function test_runwayAppearsOnlyWithoutCurrentIncident() {
        guardrails.forecasts = [forecast("warning")];
        var focus = createTemporaryObject(dailyFocusComponent, testCase, {
            presentation: presentation,
            guardrails: guardrails
        });
        verify(focus);
        compare(focus.showingRunway, true);
        compare(focus.effectiveActionRow.stableId, "openai");
        compare(focus.effectiveActionRow.nextActionKey, "review_runway");
        compare(focus.headlineText(), "Quota runway needs attention");
        compare(focus.effectiveFacts().length, 3);
    }

    function test_narrowAndWideCardsKeepAccessibleStateText() {
        var card = createTemporaryObject(runwayCardComponent, testCase, {
            forecast: forecast("safe"),
            width: 260
        });
        verify(card);
        verify(card.Accessible.name.indexOf("Safe") >= 0);
        card.width = 720;
        wait(0);
        verify(card.Accessible.name.indexOf("Safe") >= 0);
    }
}
