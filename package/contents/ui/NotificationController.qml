import QtQuick
import org.kde.notification

Item {
    id: notifications

    visible: false
    width: 0
    height: 0

    required property var configuration
    required property var registry
    required property var usageDatabase
    required property var webhookNotifier

    property var lastNotificationTimes: ({})

    readonly property string brandedNotificationIcon: "com.github.loofi.aiusagemonitor"
    readonly property string warningNotificationIcon: "dialog-warning"
    readonly property string errorNotificationIcon: "dialog-error"

    Notification {
        id: subscriptionNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "quotaWarning"
        title: i18n("AI Usage Monitor - Subscription")
        iconName: notifications.warningNotificationIcon
    }

    Notification {
        id: warningNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "quotaWarning"
        title: i18n("AI Usage Monitor")
        iconName: notifications.warningNotificationIcon
    }

    Notification {
        id: errorNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "apiError"
        title: i18n("AI Usage Monitor")
        iconName: notifications.errorNotificationIcon
    }

    Notification {
        id: budgetNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "budgetWarning"
        title: i18n("AI Usage Monitor - Budget")
        iconName: notifications.brandedNotificationIcon
    }

    Notification {
        id: connectionNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "providerDisconnected"
        title: i18n("AI Usage Monitor")
        iconName: notifications.brandedNotificationIcon
    }

    Notification {
        id: updateNotification
        componentName: "plasma_applet_com.github.loofi.aiusagemonitor"
        eventId: "updateAvailable"
        title: i18n("AI Usage Monitor - Update Available")
        iconName: notifications.brandedNotificationIcon
    }

    function canNotify(eventKey) {
        var cooldown = configuration.notificationCooldownMinutes * 60 * 1000;
        var now = Date.now();
        var last = lastNotificationTimes[eventKey] || 0;
        if (now - last < cooldown) {
            return false;
        }

        var dndStart = configuration.dndStartHour;
        var dndEnd = configuration.dndEndHour;
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

    function sendErrorNotification(title, message) {
        if (!canNotify("error_" + title)) {
            return;
        }

        errorNotification.title = title;
        errorNotification.text = message;
        errorNotification.sendEvent();
        webhookNotifier.sendAlert("error_" + title, title, message, true);
    }

    function sendUpdateAvailable(latestVersion, releaseUrl) {
        if (!configuration.notifyOnUpdate) {
            return;
        }

        updateNotification.text = i18n("Version %1 is available! Visit %2 to update.",
                                       latestVersion, releaseUrl);
        updateNotification.sendEvent();
    }

    function handleQuotaWarning(provider, percentUsed) {
        if (!configuration.alertsEnabled
                || !registry.isProviderNotificationEnabled(provider)
                || !canNotify("quota_" + provider)) {
            return;
        }

        usageDatabase.recordRateLimitEvent(provider,
            percentUsed >= configuration.criticalThreshold ? "critical" : "warning",
            percentUsed);

        if (percentUsed >= configuration.criticalThreshold) {
            warningNotification.text = i18n("%1: CRITICAL - %2% of rate limit used!", provider, percentUsed);
            warningNotification.urgency = Notification.CriticalUrgency;
            warningNotification.sendEvent();
            webhookNotifier.sendAlert("quota_" + provider,
                                      i18n("%1 rate limit critical", provider),
                                      warningNotification.text,
                                      true);
        } else if (percentUsed >= configuration.warningThreshold) {
            warningNotification.text = i18n("%1: Warning - %2% of rate limit used", provider, percentUsed);
            warningNotification.urgency = Notification.NormalUrgency;
            warningNotification.sendEvent();
            webhookNotifier.sendAlert("quota_" + provider,
                                      i18n("%1 rate limit warning", provider),
                                      warningNotification.text,
                                      false);
        }
    }

    function handleBudgetWarning(provider, period, spent, budget) {
        if (!configuration.alertsEnabled
                || !configuration.notifyOnBudgetWarning
                || !registry.isProviderNotificationEnabled(provider)
                || !canNotify("budgetwarn_" + provider + "_" + period)) {
            return;
        }

        budgetNotification.text = i18n("%1: %2 budget at %3% — $%4 / $%5",
                                       provider,
                                       period,
                                       Math.round(spent / budget * 100),
                                       spent.toFixed(2),
                                       budget.toFixed(2));
        budgetNotification.urgency = Notification.NormalUrgency;
        budgetNotification.sendEvent();
        webhookNotifier.sendAlert("budgetwarn_" + provider + "_" + period,
                                  i18n("%1 %2 budget warning", provider, period),
                                  budgetNotification.text,
                                  false);
    }

    function handleBudgetExceeded(provider, period, spent, budget) {
        if (!configuration.alertsEnabled
                || !configuration.notifyOnBudgetWarning
                || !registry.isProviderNotificationEnabled(provider)
                || !canNotify("budget_" + provider + "_" + period)) {
            return;
        }

        budgetNotification.text = i18n("%1: %2 budget exceeded! $%3 / $%4",
                                       provider, period, spent.toFixed(2), budget.toFixed(2));
        budgetNotification.urgency = Notification.CriticalUrgency;
        budgetNotification.sendEvent();
        webhookNotifier.sendAlert("budget_" + provider + "_" + period,
                                  i18n("%1 %2 budget exceeded", provider, period),
                                  budgetNotification.text,
                                  true);
    }

    function handleProviderDisconnected(provider) {
        if (!configuration.notifyOnDisconnect
                || !registry.isProviderNotificationEnabled(provider)
                || !canNotify("disconnect_" + provider)) {
            return;
        }

        connectionNotification.eventId = "providerDisconnected";
        connectionNotification.iconName = brandedNotificationIcon;
        connectionNotification.text = i18n("%1 has disconnected", provider);
        connectionNotification.urgency = Notification.NormalUrgency;
        connectionNotification.sendEvent();
        webhookNotifier.sendAlert("disconnect_" + provider,
                                  i18n("%1 disconnected", provider),
                                  connectionNotification.text,
                                  true);
    }

    function handleProviderReconnected(provider) {
        if (!configuration.notifyOnReconnect
                || !registry.isProviderNotificationEnabled(provider)
                || !canNotify("reconnect_" + provider)) {
            return;
        }

        connectionNotification.eventId = "providerReconnected";
        connectionNotification.iconName = brandedNotificationIcon;
        connectionNotification.text = i18n("%1 has reconnected", provider);
        connectionNotification.urgency = Notification.LowUrgency;
        connectionNotification.sendEvent();
        webhookNotifier.sendAlert("reconnect_" + provider,
                                  i18n("%1 reconnected", provider),
                                  connectionNotification.text,
                                  false);
    }

    function handleToolLimitWarning(toolName, percentUsed) {
        if (!configuration.alertsEnabled
                || !registry.isToolNotificationEnabled(toolName)
                || !canNotify("tool_warning_" + toolName)) {
            return;
        }

        subscriptionNotification.text = i18n("%1: %2% of usage limit reached",
                                             toolName, Math.round(percentUsed));
        subscriptionNotification.urgency = percentUsed >= configuration.criticalThreshold
            ? Notification.CriticalUrgency
            : Notification.NormalUrgency;
        subscriptionNotification.sendEvent();
        webhookNotifier.sendAlert("tool_warning_" + toolName,
                                  i18n("%1 usage warning", toolName),
                                  subscriptionNotification.text,
                                  percentUsed >= configuration.criticalThreshold);
    }

    function handleToolLimitReached(toolName) {
        if (!configuration.alertsEnabled
                || !registry.isToolNotificationEnabled(toolName)
                || !canNotify("tool_limit_" + toolName)) {
            return;
        }

        subscriptionNotification.text = i18n("%1: Usage limit reached!", toolName);
        subscriptionNotification.urgency = Notification.CriticalUrgency;
        subscriptionNotification.sendEvent();
        webhookNotifier.sendAlert("tool_limit_" + toolName,
                                  i18n("%1 usage limit reached", toolName),
                                  subscriptionNotification.text,
                                  true);
    }

    function handleToolSyncDiagnostic(toolName, code, message) {
        if (!configuration.alertsEnabled
                || !registry.isToolNotificationEnabled(toolName)
                || code === "not_logged_in"
                || code === "cookies_not_found"
                || !canNotify("tool_sync_" + toolName + "_" + code)) {
            return;
        }

        var severity = Notification.LowUrgency;
        if (code === "session_expired" || code === "format_changed" || code === "organization_missing") {
            severity = Notification.CriticalUrgency;
        } else if (code === "network_error" || code === "invalid_response" || code === "unsupported_browser") {
            severity = Notification.NormalUrgency;
        }

        subscriptionNotification.text = i18n("%1 sync: %2", toolName, message);
        subscriptionNotification.urgency = severity;
        subscriptionNotification.sendEvent();
        webhookNotifier.sendAlert("tool_sync_" + toolName + "_" + code,
                                  i18n("%1 sync diagnostic", toolName),
                                  subscriptionNotification.text,
                                  severity === Notification.CriticalUrgency);
    }
}
