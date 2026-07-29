import QtQuick
import org.kde.notification

Item {
    id: notifications

    visible: false
    width: 0
    height: 0

    required property var configuration
    required property var registry
    required property var dailyState
    required property var guardrails
    required property var usageDatabase
    required property var webhookNotifier

    // Tests observe prepared payloads without contacting the desktop
    // notification service or configured webhook endpoints.
    property bool deliveryEnabled: true
    property var lastNotificationTimes: ({})
    property var previousSourceStates: ({})

    readonly property string brandedNotificationIcon: "com.github.loofi.aiusagemonitor"
    readonly property string warningNotificationIcon: "dialog-warning"
    readonly property string errorNotificationIcon: "dialog-error"

    signal notificationPrepared(var payload)

    Notification {
        id: quotaNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "quotaWarning"
        title: i18n("AI Usage Monitor")
        iconName: notifications.warningNotificationIcon
    }

    Notification {
        id: budgetNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "budgetWarning"
        title: i18n("AI Usage Monitor")
        iconName: notifications.brandedNotificationIcon
    }

    Notification {
        id: stateNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "apiError"
        title: i18n("AI Usage Monitor")
        iconName: notifications.errorNotificationIcon
    }

    Notification {
        id: recoveryNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "providerReconnected"
        title: i18n("AI Usage Monitor")
        iconName: notifications.brandedNotificationIcon
    }

    Notification {
        id: guardrailNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "guardrailTransition"
        title: i18n("AI Usage Monitor")
        iconName: notifications.warningNotificationIcon
    }

    Notification {
        id: updateNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "updateAvailable"
        title: i18n("AI Usage Monitor - Update Available")
        iconName: notifications.brandedNotificationIcon
    }

    function canNotify(eventKey) {
        var cooldown = Math.max(0, Number(configuration.notificationCooldownMinutes || 0))
            * 60 * 1000;
        var now = Date.now();
        var last = lastNotificationTimes[eventKey] || 0;
        if (now - last < cooldown) {
            return false;
        }

        var dndStart = Number(configuration.dndStartHour);
        var dndEnd = Number(configuration.dndEndHour);
        if (dndStart >= 0 && dndEnd >= 0) {
            var hour = new Date().getHours();
            if (dndStart < dndEnd) {
                if (hour >= dndStart && hour < dndEnd) {
                    return false;
                }
            } else if (hour >= dndStart || hour < dndEnd) {
                return false;
            }
        }

        lastNotificationTimes[eventKey] = now;
        return true;
    }

    function sourceNotificationsEnabled(row) {
        if (row.sourceKind === "provider") {
            return registry.isProviderNotificationEnabled(row.displayName);
        }
        return registry.isToolNotificationEnabled(row.displayName);
    }

    function isFinitePercent(value) {
        var number = Number(value);
        return value !== undefined && value !== null
            && isFinite(number) && number >= 0 && number <= 100;
    }

    function isFiniteNonNegative(value) {
        var number = Number(value);
        return value !== undefined && value !== null
            && isFinite(number) && number >= 0;
    }

    function threshold(name, fallback) {
        var value = Number(configuration[name]);
        return isFinite(value) ? value : fallback;
    }

    function isRealFailure(row) {
        if (!row || row.freshnessState === "stale") {
            return false;
        }
        var readiness = row.readinessState || "";
        return readiness === "failed"
            || readiness === "needs_configuration"
            || readiness === "unavailable_locally"
            || readiness === "degraded";
    }

    function resetLabel(resetAt) {
        if (resetAt === undefined || resetAt === null || resetAt === "") {
            return "";
        }
        var parsed = new Date(resetAt);
        if (isNaN(parsed.getTime())) {
            return "";
        }
        return parsed.toLocaleString(Qt.locale(), Locale.ShortFormat);
    }

    function quotaSourceLabel(sourceClass) {
        switch (sourceClass) {
        case "actual":
            return i18n("Live quota");
        case "local_estimate":
            return i18n("Local estimate");
        case "configured_limit":
            return i18n("Configured limit");
        default:
            return "";
        }
    }

    function warningQuotaWindows(row) {
        if (row.freshnessState === "stale") {
            return [];
        }
        var windows = row.quotaWindows || [];
        var result = [];
        var warningRemaining = 100 - threshold("warningThreshold", 80);
        for (var i = 0; i < windows.length; i++) {
            var window = windows[i] || {};
            if (quotaSourceLabel(window.sourceClass) === ""
                    || !isFinitePercent(window.percentRemaining)
                    || Number(window.percentRemaining) > warningRemaining) {
                continue;
            }
            result.push(window);
        }
        return result;
    }

    function quotaMessage(row, windows) {
        var lines = [];
        for (var i = 0; i < windows.length; i++) {
            var window = windows[i];
            var name = window.window || window.kind || i18n("Quota window");
            var remaining = Math.round(Number(window.percentRemaining));
            var source = quotaSourceLabel(window.sourceClass);
            var reset = resetLabel(window.resetAt);
            lines.push(reset !== ""
                ? i18n("%1: %2% remaining (%3), resets %4",
                       name, remaining, source, reset)
                : i18n("%1: %2% remaining (%3)",
                       name, remaining, source));
        }
        return i18n("%1 quota status:", row.displayName) + "\n" + lines.join("\n");
    }

    function safeFailureMessage(row) {
        var reason = row.attentionReasonKey || row.lastErrorKind || "failed";
        switch (reason) {
        case "authentication":
            return i18n("%1 needs updated credentials.", row.displayName);
        case "permission":
            return i18n("%1 needs additional permission.", row.displayName);
        case "needs_configuration":
        case "configuration":
            return i18n("%1 needs configuration.", row.displayName);
        case "schema":
        case "format_changed":
            return i18n("%1 returned an unsupported data format.", row.displayName);
        default:
            return i18n("%1 could not refresh. Open the monitor for recovery steps.",
                        row.displayName);
        }
    }

    function notificationTarget(type) {
        switch (type) {
        case "quota":
            return quotaNotification;
        case "budget":
            return budgetNotification;
        case "recovery":
            return recoveryNotification;
        case "guardrail":
            return guardrailNotification;
        default:
            return stateNotification;
        }
    }

    function deliverPrepared(payload) {
        notificationPrepared(payload);
        if (!deliveryEnabled) {
            return true;
        }

        var target = notificationTarget(payload.type);
        target.title = payload.title;
        target.text = payload.message;
        target.urgency = payload.critical
            ? Notification.CriticalUrgency
            : (payload.type === "recovery"
               || payload.transition === "recovered"
               ? Notification.LowUrgency
               : Notification.NormalUrgency);
        target.sendEvent();
        if (payload.type === "guardrail"
                && webhookNotifier.sendGuardrailEvent) {
            webhookNotifier.sendGuardrailEvent(payload);
        } else {
            webhookNotifier.sendAlert(payload.eventKey,
                                      payload.title,
                                      payload.message,
                                      payload.critical);
        }
        return true;
    }

    function deliver(payload) {
        return canNotify(payload.eventKey) ? deliverPrepared(payload) : false;
    }

    function quotaPayload(row) {
        var windows = warningQuotaWindows(row);
        if (windows.length === 0) {
            return ({});
        }
        var critical = row.attentionSeverity === "critical";
        return {
            type: "quota",
            eventKey: "daily_quota_" + row.stableId,
            sourceId: row.stableId,
            actionKey: row.nextActionKey || "",
            severity: critical ? "critical" : "warning",
            title: critical
                ? i18n("%1 quota critical", row.displayName)
                : i18n("%1 quota warning", row.displayName),
            message: quotaMessage(row, windows),
            critical: critical,
            quotaWindowCount: windows.length
        };
    }

    function budgetPayload(row) {
        if (!row.budgetAvailable
                || !isFiniteNonNegative(row.budgetPercentUsed)) {
            return ({});
        }
        var critical = row.attentionSeverity === "critical";
        return {
            type: "budget",
            eventKey: "daily_budget_" + row.stableId,
            sourceId: row.stableId,
            actionKey: row.nextActionKey || "",
            severity: row.attentionSeverity,
            title: critical
                ? i18n("%1 budget critical", row.displayName)
                : i18n("%1 budget warning", row.displayName),
            message: i18n("%1 budget usage is %2%.",
                          row.displayName,
                          Math.round(Number(row.budgetPercentUsed))),
            critical: critical
        };
    }

    function failurePayload(row) {
        return {
            type: "failure",
            eventKey: "daily_failure_" + row.stableId,
            sourceId: row.stableId,
            actionKey: row.nextActionKey || "",
            severity: row.attentionSeverity,
            title: i18n("%1 needs attention", row.displayName),
            message: safeFailureMessage(row),
            critical: row.attentionSeverity === "critical"
        };
    }

    function stalePayload(row) {
        return {
            type: "stale",
            eventKey: "daily_stale_" + row.stableId,
            sourceId: row.stableId,
            actionKey: row.nextActionKey || "",
            severity: row.attentionSeverity,
            title: i18n("%1 data is stale", row.displayName),
            message: i18n("%1 kept its last useful snapshot, but it could not be refreshed.",
                          row.displayName),
            critical: false
        };
    }

    function recoveryPayload(row) {
        return {
            type: "recovery",
            eventKey: "daily_recovery_" + row.stableId,
            sourceId: row.stableId,
            actionKey: row.nextActionKey || "",
            severity: "none",
            title: i18n("%1 recovered", row.displayName),
            message: i18n("%1 is reporting again.", row.displayName),
            critical: false
        };
    }

    function processSourceRow(row) {
        if (!row || !row.stableId) {
            return false;
        }

        var previous = previousSourceStates[row.stableId];
        previousSourceStates[row.stableId] = row;
        if (!previous) {
            return false;
        }
        if (!sourceNotificationsEnabled(row)) {
            return false;
        }

        if (isRealFailure(previous) && !isRealFailure(row)) {
            return configuration.notifyOnReconnect
                ? deliver(recoveryPayload(row))
                : false;
        }
        if (!configuration.alertsEnabled) {
            return false;
        }
        if (isRealFailure(row)) {
            var reason = row.attentionReasonKey || row.lastErrorKind || "";
            var disconnected = reason === "network"
                || reason === "network_error"
                || reason === "timeout"
                || reason === "backend_unavailable";
            var failureAllowed = disconnected
                ? configuration.notifyOnDisconnect
                : configuration.notifyOnError;
            return failureAllowed ? deliver(failurePayload(row)) : false;
        }
        if (row.freshnessState === "stale"
                || row.attentionReasonKey === "stale_data") {
            return configuration.notifyOnError
                ? deliver(stalePayload(row))
                : false;
        }
        if (row.attentionReasonKey === "quota_exhausted"
                || row.attentionReasonKey === "quota_critical"
                || row.attentionReasonKey === "quota_warning") {
            var quota = quotaPayload(row);
            if (!quota.eventKey) {
                return false;
            }
            var quotaDelivered = deliver(quota);
            if (quotaDelivered && usageDatabase
                    && usageDatabase.recordRateLimitEvent) {
                usageDatabase.recordRateLimitEvent(
                    row.stableId,
                    quota.severity,
                    Math.round(Number(row.percentUsed)));
            }
            return quotaDelivered;
        }
        if ((row.attentionReasonKey === "budget_critical"
                || row.attentionReasonKey === "budget_warning")
                && configuration.notifyOnBudgetWarning) {
            var budget = budgetPayload(row);
            return budget.eventKey ? deliver(budget) : false;
        }
        return false;
    }

    function processSource(stableId) {
        if (!dailyState || !dailyState.source) {
            return false;
        }
        return processSourceRow(dailyState.source(stableId));
    }

    function synchronizeSources() {
        if (!dailyState || !dailyState.prioritizedSourceIds) {
            return;
        }
        var ids = dailyState.prioritizedSourceIds();
        var active = {};
        for (var i = 0; i < ids.length; i++) {
            var id = ids[i];
            active[id] = true;
            if (!previousSourceStates[id]) {
                previousSourceStates[id] = dailyState.source(id);
            }
        }
        var known = Object.keys(previousSourceStates);
        for (var j = 0; j < known.length; j++) {
            if (!active[known[j]]) {
                delete previousSourceStates[known[j]];
            }
        }
    }

    function guardrailSource(forecast) {
        if (!registry || !registry.providerByConfigKey) {
            return null;
        }
        return registry.providerByConfigKey(forecast.sourceId);
    }

    function guardrailSourceEnabled(forecast) {
        if (forecast.sourceKind !== "provider") {
            return false;
        }
        var source = guardrailSource(forecast);
        return !!source && source.enabled && source.notificationsEnabled;
    }

    function guardrailWithinLeadTime(forecast) {
        var predicted = new Date(forecast.predictedAt);
        if (isNaN(predicted.getTime())) {
            return false;
        }
        var leadHours = Number(configuration.forecastLeadTimeHours);
        if ([1, 6, 24, 48].indexOf(leadHours) < 0) {
            return false;
        }
        return predicted.getTime() - Date.now() <= leadHours * 60 * 60 * 1000;
    }

    function guardrailKindLabel(kind) {
        return kind === "budget_overrun"
            ? i18n("monthly budget pacing")
            : i18n("quota runway");
    }

    function guardrailValueLabel(forecast) {
        if (!isFiniteNonNegative(forecast.projectedValue)) {
            return "";
        }
        if (forecast.currency) {
            return i18n("%1 %2 projected",
                        Number(forecast.projectedValue).toFixed(2),
                        forecast.currency);
        }
        return i18n("%1 %2 projected",
                    Number(forecast.projectedValue).toFixed(0),
                    forecast.unit || "");
    }

    function guardrailPayload(forecast, transition) {
        var source = guardrailSource(forecast);
        var sourceName = source ? (source.label || source.name) : forecast.sourceId;
        var kind = guardrailKindLabel(forecast.kind);
        var critical = transition === "critical";
        var predicted = resetLabel(forecast.predictedAt);
        var message;
        if (transition === "recovered") {
            message = forecast.state === "safe"
                ? i18n("%1 %2 returned to a safe state.", sourceName, kind)
                : i18n("%1 %2 is no longer asserted; the current forecast is unavailable.",
                       sourceName, kind);
        } else {
            var value = guardrailValueLabel(forecast);
            message = predicted !== ""
                ? i18n("%1 %2 is %3. The projected event time is %4.",
                       sourceName, kind, transition, predicted)
                : i18n("%1 %2 is %3.", sourceName, kind, transition);
            if (value !== "") {
                message += " " + value + ".";
            }
        }
        return {
            type: "guardrail",
            eventKey: "guardrail_" + forecast.stableId + "_" + transition,
            sourceId: forecast.sourceId,
            kind: forecast.kind,
            state: forecast.state,
            transition: transition,
            predictedAt: forecast.predictedAt,
            periodEnd: forecast.periodEnd,
            evidenceGrade: forecast.evidenceGrade,
            methodId: forecast.methodId,
            valueClass: forecast.valueClass,
            title: transition === "recovered"
                ? i18n("%1 guardrail recovered", sourceName)
                : (critical
                   ? i18n("%1 guardrail critical", sourceName)
                   : i18n("%1 guardrail warning", sourceName)),
            message: message,
            critical: critical
        };
    }

    function processGuardrail(forecast) {
        if (!forecast || !forecast.stableId
                || !configuration.alertsEnabled
                || !configuration.forecastNotificationsEnabled
                || !guardrailSourceEnabled(forecast)
                || !usageDatabase
                || !usageDatabase.recordGuardrailTransition
                || !usageDatabase.lastGuardrailTransition) {
            return false;
        }

        var transition = "";
        if ((forecast.state === "warning" || forecast.state === "critical")
                && guardrailWithinLeadTime(forecast)) {
            transition = forecast.state;
        } else if (forecast.state === "safe"
                   || forecast.state === "unavailable") {
            var previous = usageDatabase.lastGuardrailTransition(
                forecast.stableId);
            if (previous.transition === "warning"
                    || previous.transition === "critical") {
                transition = "recovered";
            }
        }
        if (transition === "") {
            return false;
        }

        var payload = guardrailPayload(forecast, transition);
        if (!canNotify(payload.eventKey)) {
            return false;
        }
        if (!usageDatabase.recordGuardrailTransition(
                    forecast, transition)) {
            delete lastNotificationTimes[payload.eventKey];
            return false;
        }
        return deliverPrepared(payload);
    }

    function processGuardrails() {
        if (!guardrails || !guardrails.forecasts) {
            return;
        }
        var forecasts = guardrails.forecasts;
        for (var i = 0; i < forecasts.length; i++) {
            processGuardrail(forecasts[i]);
        }
    }

    function sendUpdateAvailable(latestVersion, releaseUrl) {
        if (!configuration.notifyOnUpdate) {
            return;
        }

        updateNotification.text = i18n("Version %1 is available! Visit %2 to update.",
                                       latestVersion, releaseUrl);
        updateNotification.sendEvent();
    }

    Connections {
        target: notifications.dailyState

        function onSourceChanged(stableId) {
            notifications.processSource(stableId);
        }

        function onCountChanged() {
            notifications.synchronizeSources();
        }
    }

    Connections {
        target: notifications.guardrails

        function onForecastsChanged() {
            notifications.processGuardrails();
        }
    }

    Component.onCompleted: {
        Qt.callLater(synchronizeSources);
        Qt.callLater(processGuardrails);
    }
}
