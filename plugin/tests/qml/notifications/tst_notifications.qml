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
    }

    QtObject {
        id: fakeDailyState
        signal sourceChanged(string stableId)
        signal countChanged()
        function prioritizedSourceIds() { return []; }
        function source(stableId) { return ({}); }
    }

    QtObject {
        id: fakeUsageDatabase
        property int recordedEvents: 0
        function recordRateLimitEvent(source, severity, percentUsed) {
            recordedEvents++;
        }
    }

    QtObject {
        id: fakeWebhookNotifier
        property int sentAlerts: 0
        function sendAlert(eventKey, title, message, critical) {
            sentAlerts++;
        }
    }

    Component {
        id: controllerComponent
        Monitor.NotificationController {
            configuration: fakeConfiguration
            registry: fakeRegistry
            dailyState: fakeDailyState
            usageDatabase: fakeUsageDatabase
            webhookNotifier: fakeWebhookNotifier
            deliveryEnabled: false
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
        fakeUsageDatabase.recordedEvents = 0;
        fakeWebhookNotifier.sentAlerts = 0;
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
}
