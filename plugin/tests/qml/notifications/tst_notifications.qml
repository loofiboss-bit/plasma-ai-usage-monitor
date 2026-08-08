import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor

TestCase {
    id: testCase
    name: "DailyNotifications"

    function i18n(message) {
        var result = message;
        for (var i = 1; i < arguments.length; i++)
            result = result.replace("%" + i, arguments[i]);
        return result;
    }

    QtObject {
        id: fakeConfiguration
        property bool alertsEnabled: true
        property bool notifyOnError: true
        property bool notifyOnBudgetWarning: true
        property bool notifyOnDisconnect: true
        property bool notifyOnReconnect: true
        property bool notifyOnUpdate: false
        property bool forecastNotificationsEnabled: true
        property int forecastLeadTimeHours: 24
        property int warningThreshold: 80
        property int criticalThreshold: 95
        property int notificationCooldownMinutes: 15
        property int dndStartHour: -1
        property int dndEndHour: -1
    }

    QtObject {
        id: fakeRegistry
        function isProviderNotificationEnabled(displayName) { return true; }
        function isToolNotificationEnabled(displayName) { return true; }
        function providerByConfigKey(configKey) {
            return configKey === "openai"
                ? {
                    enabled: true,
                    notificationsEnabled: true,
                    label: "OpenAI",
                    name: "OpenAI"
                }
                : null;
        }
    }

    QtObject {
        id: fakeDailyState
        signal sourceChanged(string stableId)
        signal countChanged()
        function prioritizedSourceIds() { return []; }
        function source(stableId) { return ({}); }
    }

    QtObject {
        id: fakeGuardrails
        property var forecasts: []
    }

    QtObject {
        id: fakeUsageDatabase
        property int recordedEvents: 0
        property var transitions: ({})
        function recordRateLimitEvent(source, severity, percentUsed) {
            recordedEvents++;
        }
        function recordGuardrailTransition(forecast, transition) {
            var previous = transitions[forecast.stableId];
            if (previous && previous.transition === transition)
                return false;
            transitions[forecast.stableId] = { transition: transition };
            recordedEvents++;
            return true;
        }
        function lastGuardrailTransition(stableId) {
            return transitions[stableId] || ({});
        }
    }

    QtObject {
        id: fakeWebhookNotifier
        property int sentAlerts: 0
        function sendAlert(eventKey, title, message, critical) {
            sentAlerts++;
        }
        function sendGuardrailEvent(event) {
            sentAlerts++;
        }
    }

    QtObject {
        id: fakeBudgetPolicyRepository
        property int nextEventId: 1
        property string previousState: ""
        property var pendingEvent: null
        property var sequence: []
        function isRisky(state) {
            return ["warning", "critical", "exceeded"].indexOf(state) >= 0;
        }
        function prepareTransitions(row, suppressionReason) {
            sequence.push("persist");
            if (pendingEvent && pendingEvent.status === "pending") {
                pendingEvent.deliver = suppressionReason === "";
                return { ok: true, events: [pendingEvent] };
            }
            var transition = "";
            if (isRisky(row.state) && previousState !== row.state)
                transition = row.state;
            else if (row.state === "safe" && isRisky(previousState))
                transition = "recovered";
            previousState = row.state;
            if (transition === "") return { ok: true, events: [] };
            var status = suppressionReason === "" || suppressionReason === "dnd"
                ? "pending" : "suppressed";
            pendingEvent = {
                eventId: nextEventId++,
                policyId: row.policyId,
                transition: transition,
                periodStart: row.periodStart,
                periodEnd: row.periodEnd,
                deliveryStatus: status,
                status: status,
                deliver: status === "pending" && suppressionReason === ""
            };
            return { ok: true, events: [pendingEvent] };
        }
        function markEventDelivered(eventId) {
            sequence.push("delivered");
            pendingEvent.status = "delivered";
            return true;
        }
        function markEventFailed(eventId, reasonKey) {
            sequence.push("failed");
            pendingEvent.status = "failed";
            return true;
        }
    }

    Component {
        id: controllerComponent
        Monitor.NotificationController {
            configuration: fakeConfiguration
            registry: fakeRegistry
            dailyState: fakeDailyState
            guardrails: fakeGuardrails
            usageDatabase: fakeUsageDatabase
            webhookNotifier: fakeWebhookNotifier
            budgetPolicyRepository: fakeBudgetPolicyRepository
            deliveryEnabled: false
            onNotificationPrepared: fakeBudgetPolicyRepository.sequence.push("notify")
        }
    }

    property var controller

    SignalSpy {
        id: notificationSpy
        target: controller
        signalName: "notificationPrepared"
    }

    function sourceRow(options) {
        var values = options || {};
        return {
            stableId: values.stableId || "openai",
            displayName: values.displayName || "OpenAI",
            sourceKind: values.sourceKind || "provider",
            readinessState: values.readinessState || "reporting_actual",
            freshnessState: values.freshnessState || "fresh",
            attentionSeverity: values.attentionSeverity || "none",
            attentionReasonKey: values.attentionReasonKey || "none",
            lastErrorKind: values.lastErrorKind || "",
            nextActionKey: values.nextActionKey || "none",
            percentUsedAvailable: values.percentUsedAvailable === true,
            percentUsed: values.percentUsed,
            budgetAvailable: values.budgetAvailable === true,
            budgetPercentUsed: values.budgetPercentUsed,
            quotaWindows: values.quotaWindows || []
        };
    }

    function quota(window, remaining, sourceClass, resetAt) {
        var result = {
            kind: window,
            window: window,
            percentUsed: 100 - remaining,
            percentRemaining: remaining,
            sourceClass: sourceClass,
            sourceKey: sourceClass === "actual"
                ? "response_headers"
                : "local_observation"
        };
        if (resetAt !== undefined)
            result.resetAt = resetAt;
        return result;
    }

    function forecast(state, options) {
        var values = options || {};
        return {
            stableId: values.stableId || "forecast-contract-stable-id",
            kind: values.kind || "quota_exhaustion",
            state: state,
            sourceId: "openai",
            sourceKind: "provider",
            window: values.window || "requests_24h",
            scope: values.scope || "organization",
            currentValue: values.currentValue === undefined ? 40 : values.currentValue,
            projectedValue: values.projectedValue === undefined ? 100 : values.projectedValue,
            limitValue: values.limitValue === undefined ? 100 : values.limitValue,
            unit: values.unit || "requests",
            currency: values.currency,
            predictedAt: values.predictedAt === undefined
                ? new Date(Date.now() + 60 * 60 * 1000)
                : values.predictedAt,
            periodEnd: values.periodEnd
                || new Date(Date.now() + 24 * 60 * 60 * 1000),
            sampleCount: 8,
            coveragePercent: 100,
            evidenceGrade: "strong",
            methodId: values.methodId || "quota_theil_sen_v1",
            reasonKey: state === "unavailable" ? "stale_data" : "",
            generatedAt: new Date(),
            valueClass: values.valueClass || "actual"
        };
    }

    function budgetPolicyForecast(state, options) {
        var values = options || {};
        var start = values.periodStart || new Date("2026-08-01T00:00:00Z");
        return {
            contractVersion: "budget-pacing-v2",
            stableId: "local-stable-hash",
            policyId: values.policyId || "11111111-1111-4111-8111-111111111111",
            kind: "budget_overrun",
            state: state,
            sourceId: "openai",
            sourceKind: "provider",
            window: values.window || "calendar_month",
            periodStart: start,
            periodEnd: values.periodEnd || new Date("2026-09-01T00:00:00Z"),
            generatedAt: new Date("2026-08-20T00:00:00Z"),
            consumedPercent: values.consumedPercent === undefined
                ? 85 : values.consumedPercent,
            valueClass: "actual"
        };
    }

    function payload(index) {
        return notificationSpy.signalArguments[index][0];
    }

    function init() {
        fakeConfiguration.alertsEnabled = true;
        fakeConfiguration.notifyOnError = true;
        fakeConfiguration.notifyOnBudgetWarning = true;
        fakeConfiguration.notifyOnDisconnect = true;
        fakeConfiguration.notifyOnReconnect = true;
        fakeConfiguration.warningThreshold = 80;
        fakeConfiguration.criticalThreshold = 95;
        fakeConfiguration.notificationCooldownMinutes = 15;
        fakeConfiguration.dndStartHour = -1;
        fakeConfiguration.dndEndHour = -1;
        fakeConfiguration.forecastNotificationsEnabled = true;
        fakeConfiguration.forecastLeadTimeHours = 24;
        fakeUsageDatabase.recordedEvents = 0;
        fakeUsageDatabase.transitions = ({});
        fakeGuardrails.forecasts = [];
        fakeWebhookNotifier.sentAlerts = 0;
        fakeBudgetPolicyRepository.nextEventId = 1;
        fakeBudgetPolicyRepository.previousState = "";
        fakeBudgetPolicyRepository.pendingEvent = null;
        fakeBudgetPolicyRepository.sequence = [];
        controller = createTemporaryObject(controllerComponent, testCase);
        verify(controller);
        notificationSpy.target = controller;
        notificationSpy.clear();
    }

    function cleanup() {
        notificationSpy.target = null;
        controller.destroy();
        controller = null;
    }

    function test_groupedQuotaUsesDailySeverityAndTruthLabels() {
        controller.processSourceRow(sourceRow({}));
        var reset = new Date(Date.now() + 60 * 60 * 1000);
        controller.processSourceRow(sourceRow({
            attentionSeverity: "critical",
            attentionReasonKey: "quota_critical",
            nextActionKey: "review_quota",
            percentUsedAvailable: true,
            percentUsed: 97,
            quotaWindows: [
                quota("Requests", 3, "actual", reset),
                quota("Weekly", 10, "local_estimate")
            ]
        }));

        compare(notificationSpy.count, 1);
        compare(payload(0).type, "quota");
        compare(payload(0).sourceId, "openai");
        compare(payload(0).severity, "critical");
        compare(payload(0).actionKey, "review_quota");
        compare(payload(0).quotaWindowCount, 2);
        verify(payload(0).message.indexOf("Live quota") >= 0);
        verify(payload(0).message.indexOf("Local estimate") >= 0);
        compare(payload(0).message.split("resets").length - 1, 1);
        compare(fakeUsageDatabase.recordedEvents, 1);
        compare(fakeWebhookNotifier.sentAlerts, 0);
    }

    function test_unknownAndUnavailableQuotaNeverAlert() {
        controller.processSourceRow(sourceRow({}));
        controller.processSourceRow(sourceRow({
            attentionSeverity: "critical",
            attentionReasonKey: "quota_exhausted",
            quotaWindows: [
                quota("Unknown", 0, "unknown"),
                {
                    kind: "Missing",
                    window: "Missing",
                    sourceClass: "actual"
                }
            ]
        }));
        compare(notificationSpy.count, 0);
        compare(fakeUsageDatabase.recordedEvents, 0);
    }

    function test_cooldownKeepsOneNotificationPerSource() {
        var healthy = sourceRow({});
        var warning = sourceRow({
            attentionSeverity: "warning",
            attentionReasonKey: "quota_warning",
            percentUsedAvailable: true,
            percentUsed: 85,
            quotaWindows: [quota("Rolling", 15, "actual")]
        });
        controller.processSourceRow(healthy);
        controller.processSourceRow(warning);
        controller.processSourceRow(warning);
        compare(notificationSpy.count, 1);
        compare(fakeUsageDatabase.recordedEvents, 1);
    }

    function test_dndSuppressesPreparedAndWebhookPayloads() {
        var hour = new Date().getHours();
        fakeConfiguration.dndStartHour = hour;
        fakeConfiguration.dndEndHour = (hour + 1) % 24;
        controller.processSourceRow(sourceRow({}));
        controller.processSourceRow(sourceRow({
            attentionSeverity: "warning",
            attentionReasonKey: "quota_warning",
            percentUsedAvailable: true,
            percentUsed: 85,
            quotaWindows: [quota("Rolling", 15, "actual")]
        }));
        compare(notificationSpy.count, 0);
        compare(fakeWebhookNotifier.sentAlerts, 0);
        compare(fakeUsageDatabase.recordedEvents, 0);
    }

    function test_budgetAboveOneHundredRemainsAlertable() {
        controller.processSourceRow(sourceRow({}));
        controller.processSourceRow(sourceRow({
            attentionSeverity: "critical",
            attentionReasonKey: "budget_critical",
            budgetAvailable: true,
            budgetPercentUsed: 125
        }));
        compare(notificationSpy.count, 1);
        compare(payload(0).type, "budget");
        compare(payload(0).severity, "critical");
        verify(payload(0).message.indexOf("125%") >= 0);
    }

    function test_recoveryRequiresPreviousRealFailure() {
        fakeConfiguration.alertsEnabled = false;
        var healthy = sourceRow({});
        var stale = sourceRow({
            freshnessState: "stale",
            attentionSeverity: "warning",
            attentionReasonKey: "stale_data"
        });
        var failed = sourceRow({
            readinessState: "failed",
            attentionSeverity: "critical",
            attentionReasonKey: "authentication",
            lastErrorKind: "authentication"
        });

        controller.processSourceRow(stale);
        controller.processSourceRow(healthy);
        compare(notificationSpy.count, 0);
        controller.processSourceRow(failed);
        compare(notificationSpy.count, 0);
        controller.processSourceRow(healthy);
        compare(notificationSpy.count, 1);
        compare(payload(0).type, "recovery");
    }

    function test_staleSnapshotCannotBecomeFalseQuotaChange() {
        controller.processSourceRow(sourceRow({}));
        controller.processSourceRow(sourceRow({
            freshnessState: "stale",
            attentionSeverity: "warning",
            attentionReasonKey: "stale_data",
            percentUsedAvailable: true,
            percentUsed: 100,
            quotaWindows: [quota("Cached", 0, "actual")]
        }));
        compare(notificationSpy.count, 1);
        compare(payload(0).type, "stale");
        verify(payload(0).message.indexOf("0%") < 0);
    }

    function test_failurePayloadIsSourceExplicitAndRedacted() {
        controller.processSourceRow(sourceRow({}));
        controller.processSourceRow(sourceRow({
            readinessState: "failed",
            attentionSeverity: "critical",
            attentionReasonKey: "authentication",
            lastErrorKind: "authentication",
            rawError: "token=super-secret"
        }));
        compare(notificationSpy.count, 1);
        compare(payload(0).sourceId, "openai");
        verify(payload(0).message.indexOf("OpenAI") >= 0);
        verify(payload(0).message.indexOf("super-secret") < 0);
    }

    function test_guardrailTransitionsAreOptInAndRestartDeduplicated() {
        fakeConfiguration.forecastNotificationsEnabled = false;
        verify(!controller.processGuardrail(forecast("warning")));
        compare(fakeUsageDatabase.recordedEvents, 0);

        fakeConfiguration.forecastNotificationsEnabled = true;
        verify(controller.processGuardrail(forecast("warning")));
        verify(!controller.processGuardrail(forecast("warning")));
        verify(controller.processGuardrail(forecast("critical")));
        verify(controller.processGuardrail(forecast("safe", {
            predictedAt: null
        })));
        verify(!controller.processGuardrail(forecast("safe", {
            predictedAt: null
        })));
        compare(notificationSpy.count, 3);
        compare(fakeUsageDatabase.recordedEvents, 3);
        compare(payload(0).transition, "warning");
        compare(payload(1).transition, "critical");
        compare(payload(2).transition, "recovered");

        notificationSpy.target = null;
        controller.destroy();
        controller = createTemporaryObject(controllerComponent, testCase);
        verify(controller);
        notificationSpy.target = controller;
        notificationSpy.clear();
        verify(!controller.processGuardrail(forecast("safe", {
            predictedAt: null
        })));
        compare(notificationSpy.count, 0);
        compare(fakeUsageDatabase.recordedEvents, 3);
    }

    function test_guardrailLeadTimeAndPayloadPrivacy() {
        var later = forecast("warning", {
            predictedAt: new Date(Date.now() + 25 * 60 * 60 * 1000),
            scope: "project:secret-project",
            modelScope: "secret-model",
            projectScope: "secret-project"
        });
        verify(!controller.processGuardrail(later));
        compare(fakeUsageDatabase.recordedEvents, 0);

        later.predictedAt = new Date(Date.now() + 23 * 60 * 60 * 1000);
        verify(controller.processGuardrail(later));
        compare(notificationSpy.count, 1);
        var event = payload(0);
        compare(event.type, "guardrail");
        compare(event.sourceId, "openai");
        verify(event.scope === undefined);
        verify(event.modelScope === undefined);
        verify(event.projectScope === undefined);
        verify(event.message.indexOf("secret-project") < 0);
        verify(event.message.indexOf("secret-model") < 0);
    }

    function test_budgetGuardrailUsesForecastContractKind() {
        var budget = forecast("warning", {
            stableId: "budget-forecast-contract-stable-id",
            kind: "budget_overrun",
            window: "calendar_month",
            unit: "USD",
            currency: "USD",
            projectedValue: 18.72,
            methodId: "budget-pacing-v1"
        });

        verify(controller.processGuardrail(budget));
        compare(notificationSpy.count, 1);
        compare(payload(0).kind, "budget_overrun");
        verify(payload(0).message.indexOf("monthly budget pacing") >= 0);
        verify(payload(0).message.indexOf("quota runway") < 0);
    }

    function test_budgetPolicyPersistsBeforeDeliveryAndUsesTypedPayload() {
        verify(controller.processGuardrail(budgetPolicyForecast("warning")));
        compare(fakeBudgetPolicyRepository.sequence.join(","),
                "persist,notify,delivered");
        compare(notificationSpy.count, 1);
        var event = payload(0);
        compare(event.contractVersion, "budget-pacing-v2");
        compare(event.transition, "warning");
        compare(event.percentClass, "warning");
        compare(event.period, "calendar_month");
        compare(event.linkText, "Open Budget Control");
        verify(event.message.indexOf(event.policyId) < 0);
    }

    function test_budgetPolicyDndIsPendingAndRetriedWithoutDuplication() {
        var hour = new Date().getHours();
        fakeConfiguration.dndStartHour = hour;
        fakeConfiguration.dndEndHour = (hour + 1) % 24;
        verify(!controller.processGuardrail(budgetPolicyForecast("warning")));
        compare(fakeBudgetPolicyRepository.pendingEvent.status, "pending");
        compare(notificationSpy.count, 0);

        fakeConfiguration.dndStartHour = -1;
        fakeConfiguration.dndEndHour = -1;
        verify(controller.processGuardrail(budgetPolicyForecast("warning")));
        compare(fakeBudgetPolicyRepository.nextEventId, 2);
        compare(notificationSpy.count, 1);
    }

    function test_budgetPolicyFailedDeliveryIsPersistedAndTerminal() {
        controller.injectDeliveryFailure = true;
        verify(!controller.processGuardrail(budgetPolicyForecast("exceeded", {
            consumedPercent: 125
        })));
        compare(fakeBudgetPolicyRepository.sequence.join(","),
                "persist,notify,failed");
        compare(fakeBudgetPolicyRepository.pendingEvent.status, "failed");
        compare(notificationSpy.count, 1);
        verify(!controller.processGuardrail(budgetPolicyForecast("exceeded", {
            consumedPercent: 125
        })));
        compare(fakeBudgetPolicyRepository.nextEventId, 2);
        compare(notificationSpy.count, 1);
    }
}
