pragma ComponentBehavior: Bound

import QtQuick
import com.github.loofi.aiusagemonitor 1.0

Item {
    id: scheduler

    visible: false
    width: 0
    height: 0

    required property var configuration
    required property var registry
    required property var browserSyncService
    required property var claudeCodeMonitor
    required property var codexCliMonitor
    required property var copilotMonitor
    required property var antigravityMonitor
    required property var usageDatabase
    required property bool popupOpen
    property alias networkPolicy: refreshPolicy

    readonly property int refreshStartup: 0
    readonly property int refreshScheduled: 1
    readonly property int refreshPopupOpened: 2
    readonly property int refreshManual: 3
    readonly property int refreshConfigurationChanged: 4
    readonly property int refreshCredentialChanged: 5

    RefreshSchedulerModel {
        id: refreshPolicy
        Component.onCompleted: startMonitoring()
        onRecoveryRequested: {
            scheduler.refreshStaleProviders(scheduler.refreshScheduled);
            scheduler.refreshAntigravity(false);
            scheduler.performAutomaticSubscriptionSync();
        }
    }

    onPopupOpenChanged: {
        if (popupOpen) {
            refreshStaleProviders(refreshPopupOpened);
            refreshAntigravity(false);
        }
    }

    function effectiveInterval(providerInterval) {
        return refreshPolicy.effectiveIntervalMs(providerInterval || 0,
                                                 configuration.refreshInterval || 60,
                                                 popupOpen);
    }

    function constrainedProviderInterval(provider) {
        return Math.max(provider?.refreshInterval || 0,
                        provider?.minimumRefreshSeconds || 0);
    }

    function backoffMultiplier(provider) {
        if (!provider || !provider.backend) {
            return 1;
        }
        var errors = provider.backend.consecutiveErrors || 0;
        if (errors <= 0) {
            return 1;
        }
        return refreshPolicy.backoffMultiplier(errors, provider.backend.retryable);
    }

    function scheduledInterval(provider) {
        return refreshPolicy.scheduledIntervalMs(provider.configKey || "",
                                                  constrainedProviderInterval(provider),
                                                  configuration.refreshInterval || 60,
                                                  popupOpen,
                                                  provider.backend?.consecutiveErrors || 0,
                                                  provider.backend?.retryable || false);
    }

    function isLoopbackProvider(provider) {
        if (provider?.configKey === "ollama") return true;
        var url = String(provider?.backend?.customBaseUrl || "").trim();
        return /^https?:\/\/(?:localhost|127(?:\.\d{1,3}){3}|\[?::1\]?)(?::|\/|$)/i.test(url);
    }

    function providerRefreshAllowed(provider) {
        return refreshPolicy.externalRefreshAllowed || isLoopbackProvider(provider);
    }

    function providerDueAt(provider) {
        if (!provider || !provider.enabled || !provider.backend
                || !canRefreshBackend(provider.backend, provider.requiresApiKey !== false)
                || provider.backend.loading || !providerRefreshAllowed(provider)) {
            return null;
        }

        var backend = provider.backend;
        var failures = Number(backend.consecutiveErrors || 0);
        if (failures > 0 && backend.retryable !== true) return null;

        var base = failures > 0 ? backend.lastAttempt : backend.lastSuccess;
        if (!base || !new Date(base).getTime()) return null;
        var due = new Date(base).getTime() + scheduledInterval(provider);
        var retryAt = backend.retryAfter ? new Date(backend.retryAfter).getTime() : 0;
        if (retryAt > due) due = retryAt;
        return new Date(due);
    }

    function canRefreshBackend(backend, requiresApiKey) {
        return backend && (!requiresApiKey || backend.hasApiKey()
                           || backend.adminApiKeyConfigured === true);
    }

    function isFresh(provider) {
        if (!provider || !provider.backend || !provider.backend.lastSuccess) {
            return false;
        }
        return refreshPolicy.isFresh(provider.backend.lastSuccess,
                                     constrainedProviderInterval(provider),
                                     configuration.refreshInterval || 60,
                                     popupOpen);
    }

    function refreshProvider(provider, reason, force) {
        if (!provider || !provider.enabled || !provider.backend) {
            return;
        }
        var refreshReason = reason === undefined ? refreshManual : reason;
        if (refreshReason !== refreshManual && !providerRefreshAllowed(provider))
            return;
        if (!force && isFresh(provider)) {
            return;
        }
        if (canRefreshBackend(provider.backend, provider.requiresApiKey !== false)) {
            provider.backend.requestRefresh(refreshReason);
        }
    }

    function refreshAll(reason) {
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            refreshProvider(providers[i], reason === undefined ? refreshManual : reason, true);
        }
    }

    function refreshStaleProviders(reason) {
        var providers = registry.allProviders || [];
        for (var i = 0; i < providers.length; i++) {
            refreshProvider(providers[i], reason, false);
        }
    }

    function performBrowserSync() {
        if (!configuration.browserSyncEnabled) {
            return;
        }

        if (configuration.claudeCodeEnabled && claudeCodeMonitor.installed && claudeCodeMonitor.canAutoSync()) {
            browserSyncService.sync("claude", claudeCodeMonitor);
        }

        if (configuration.codexEnabled && codexCliMonitor.installed && codexCliMonitor.canAutoSync()) {
            browserSyncService.sync("codex", codexCliMonitor);
        }
    }

    function performAutomaticSubscriptionSync() {
        if (refreshPolicy.externalRefreshAllowed
                && configuration.browserSyncEnabled && configuration.claudeCodeEnabled
                && claudeCodeMonitor.installed && claudeCodeMonitor.canAutoSync()) {
            browserSyncService.sync("claude", claudeCodeMonitor);
        }

        if (configuration.codexEnabled && codexCliMonitor.installed
                && codexCliMonitor.canAutoSyncFromLocalAuth()) {
            codexCliMonitor.syncFromLocalAuth();
        }
    }

    function antigravityIsFresh() {
        var last = antigravityMonitor?.lastSuccessfulRefresh;
        if (!last) return false;
        var ageMs = Date.now() - new Date(last).getTime();
        return ageMs < Math.max(60, configuration.antigravityRefreshInterval || 300) * 1000;
    }

    function refreshAntigravity(force) {
        if (!configuration.antigravityEnabled || !antigravityMonitor) return;
        if (force || !antigravityIsFresh()) antigravityMonitor.refreshQuota();
    }

    function fetchCopilotOrgMetrics(force) {
        if (!configuration.copilotEnabled || !copilotMonitor
                || !copilotMonitor.githubToken || !copilotMonitor.orgName) return;
        if (!force && !refreshPolicy.externalRefreshAllowed) return;
        copilotMonitor.fetchOrgMetrics();
    }

    function publishSubscriptionDeadline(monitor, dueAt) {
        if (monitor && monitor.setNextScheduledRefresh)
            monitor.setNextScheduledRefresh(dueAt);
    }

    Instantiator {
        model: scheduler.registry.allProviders

        delegate: Item {
            id: providerDelegate
            visible: false
            width: 0
            height: 0
            required property var modelData

            Timer {
                id: providerTimer
                property date dueAt: new Date(NaN)
                interval: dueAt && isFinite(dueAt.getTime()) ? Math.max(1, dueAt.getTime() - Date.now()) : 1
                running: dueAt && isFinite(dueAt.getTime())
                repeat: false
                onTriggered: scheduler.refreshProvider(providerDelegate.modelData, scheduler.refreshScheduled, true)

                function updateNextSchedule() {
                    var provider = providerDelegate.modelData;
                    if (!provider.backend) return;
                    var nextDue = scheduler.providerDueAt(provider);
                    dueAt = nextDue ? nextDue : new Date(NaN);
                    provider.backend.setNextScheduledRefresh(dueAt);
                }
            }

            Connections {
                target: providerDelegate.modelData.backend
                function onStateChanged() {
                    providerTimer.updateNextSchedule();
                }
                function onErrorChanged() {
                    providerTimer.updateNextSchedule();
                }
                function onDataUpdated() {
                    providerTimer.updateNextSchedule();
                }
            }

            Connections {
                target: scheduler.networkPolicy
                function onReachabilityChanged() { providerTimer.updateNextSchedule(); }
            }

            Connections {
                target: scheduler
                function onPopupOpenChanged() { providerTimer.updateNextSchedule(); }
            }

            Connections {
                target: scheduler.configuration
                function onRefreshIntervalChanged() { providerTimer.updateNextSchedule(); }
            }

            Component.onCompleted: providerTimer.updateNextSchedule()
        }
    }

    Timer {
        id: claudeSyncTimer
        property date dueAt: new Date(NaN)
        interval: dueAt && isFinite(dueAt.getTime()) ? Math.max(1, dueAt.getTime() - Date.now()) : 1
        running: dueAt && isFinite(dueAt.getTime()) && scheduler.networkPolicy.externalRefreshAllowed
        repeat: false
        function updateNextSchedule() {
            var monitor = scheduler.claudeCodeMonitor;
            if (!scheduler.configuration.browserSyncEnabled || !scheduler.configuration.claudeCodeEnabled
                    || !monitor.installed || monitor.syncNeedsAction || monitor.syncing
                    || !scheduler.networkPolicy.externalRefreshAllowed) {
                dueAt = new Date(NaN);
                scheduler.publishSubscriptionDeadline(monitor, dueAt);
                return;
            }
            var intervalMs = Math.max(60, scheduler.configuration.browserSyncInterval) * 1000;
            var jitter = scheduler.networkPolicy.deterministicJitterMs("claude-code");
            var successAt = new Date(monitor.lastSyncTime).getTime();
            var completionAt = new Date(monitor.lastSyncCompletionTime).getTime();
            var base = isFinite(successAt) && successAt > completionAt ? successAt : completionAt;
            var due = isFinite(base) && base > 0
                ? base + intervalMs + jitter : Date.now() + intervalMs + jitter;
            var retry = new Date(monitor.syncRetryAfter).getTime();
            if (!isFinite(retry)) retry = 0;
            dueAt = new Date(Math.max(due, retry));
            scheduler.publishSubscriptionDeadline(monitor, dueAt);
        }
        onTriggered: {
            if (scheduler.configuration.browserSyncEnabled && scheduler.configuration.claudeCodeEnabled
                    && scheduler.claudeCodeMonitor.canAutoSync()) {
                scheduler.browserSyncService.sync("claude", scheduler.claudeCodeMonitor);
            }
            updateNextSchedule();
        }
        Component.onCompleted: updateNextSchedule()
    }

    Connections {
        target: scheduler.claudeCodeMonitor
        function onSyncStatusChanged() { claudeSyncTimer.updateNextSchedule(); }
        function onInstalledChanged() { claudeSyncTimer.updateNextSchedule(); }
    }

    Timer {
        id: codexSyncTimer
        property date dueAt: new Date(NaN)
        interval: dueAt && isFinite(dueAt.getTime()) ? Math.max(1, dueAt.getTime() - Date.now()) : 1
        running: dueAt && isFinite(dueAt.getTime())
        repeat: false
        function updateNextSchedule() {
            var monitor = scheduler.codexCliMonitor;
            if (!scheduler.configuration.codexEnabled || !monitor.installed || monitor.syncNeedsAction
                    || monitor.syncing) {
                dueAt = new Date(NaN);
                scheduler.publishSubscriptionDeadline(monitor, dueAt);
                return;
            }
            var intervalMs = Math.max(60, scheduler.configuration.browserSyncInterval) * 1000;
            var jitter = scheduler.networkPolicy.deterministicJitterMs("codex-cli");
            var successAt = new Date(monitor.lastSyncTime).getTime();
            var completionAt = new Date(monitor.lastSyncCompletionTime).getTime();
            var base = isFinite(successAt) && successAt > completionAt ? successAt : completionAt;
            var due = isFinite(base) && base > 0
                ? base + intervalMs + jitter : Date.now() + intervalMs + jitter;
            var retry = new Date(monitor.syncRetryAfter).getTime();
            if (!isFinite(retry)) retry = 0;
            dueAt = new Date(Math.max(due, retry));
            scheduler.publishSubscriptionDeadline(monitor, dueAt);
        }
        onTriggered: {
            if (scheduler.configuration.codexEnabled && scheduler.codexCliMonitor.canAutoSyncFromLocalAuth())
                scheduler.codexCliMonitor.syncFromLocalAuth();
            updateNextSchedule();
        }
        Component.onCompleted: updateNextSchedule()
    }

    Connections {
        target: scheduler.codexCliMonitor
        function onSyncStatusChanged() { codexSyncTimer.updateNextSchedule(); }
        function onInstalledChanged() { codexSyncTimer.updateNextSchedule(); }
    }

    Connections {
        target: scheduler.networkPolicy
        function onReachabilityChanged() {
            claudeSyncTimer.updateNextSchedule();
            codexSyncTimer.updateNextSchedule();
            antigravityTimer.updateNextSchedule();
            copilotTimer.updateNextSchedule();
        }
    }

    Connections {
        target: scheduler.configuration
        function onBrowserSyncEnabledChanged() { claudeSyncTimer.updateNextSchedule(); }
        function onClaudeCodeEnabledChanged() { claudeSyncTimer.updateNextSchedule(); }
        function onCodexEnabledChanged() { codexSyncTimer.updateNextSchedule(); }
        function onBrowserSyncIntervalChanged() {
            claudeSyncTimer.updateNextSchedule();
            codexSyncTimer.updateNextSchedule();
        }
        function onAntigravityEnabledChanged() { antigravityTimer.updateNextSchedule(); }
        function onAntigravityRefreshIntervalChanged() { antigravityTimer.updateNextSchedule(); }
        function onCopilotEnabledChanged() { copilotTimer.updateNextSchedule(); }
    }

    Timer {
        id: antigravityTimer
        property date dueAt: new Date(NaN)
        property int failureCount: 0
        interval: dueAt && isFinite(dueAt.getTime()) ? Math.max(1, dueAt.getTime() - Date.now()) : 1
        running: dueAt && isFinite(dueAt.getTime())
        repeat: false
        function updateNextSchedule() {
            var monitor = scheduler.antigravityMonitor;
            var needsUserAction = ["not_installed", "daemon_not_running",
                "not_signed_in", "unsupported_version"].indexOf(monitor?.readinessCode || "") >= 0;
            if (!scheduler.configuration.antigravityEnabled || !monitor
                    || !monitor.installed || needsUserAction || monitor.syncing
                    || !scheduler.networkPolicy.externalRefreshAllowed) {
                dueAt = new Date(NaN);
                scheduler.publishSubscriptionDeadline(monitor, dueAt);
                return;
            }
            var intervalMs = Math.max(60, scheduler.configuration.antigravityRefreshInterval || 300) * 1000;
            var successAt = new Date(monitor.lastSyncTime).getTime();
            var completionAt = new Date(monitor.lastSyncCompletionTime).getTime();
            var base = failureCount > 0 ? completionAt : successAt;
            var backoff = Math.min(8, Math.pow(2, Math.min(failureCount, 3)));
            var jitter = scheduler.networkPolicy.deterministicJitterMs("google-antigravity");
            var due = isFinite(base) && base > 0
                ? base + intervalMs * backoff + jitter
                : Date.now() + intervalMs + jitter;
            var retry = new Date(monitor.syncRetryAfter).getTime();
            if (!isFinite(retry)) retry = 0;
            dueAt = new Date(Math.max(due, retry));
            scheduler.publishSubscriptionDeadline(monitor, dueAt);
        }
        onTriggered: {
            if (scheduler.networkPolicy.externalRefreshAllowed)
                scheduler.refreshAntigravity(true);
            updateNextSchedule();
        }
        Component.onCompleted: updateNextSchedule()
    }

    Connections {
        target: scheduler.antigravityMonitor
        function onAntigravityStatusChanged() { antigravityTimer.updateNextSchedule(); }
        function onSyncStatusChanged() { antigravityTimer.updateNextSchedule(); }
        function onSyncCompleted(success) {
            antigravityTimer.failureCount = success ? 0 : antigravityTimer.failureCount + 1;
            antigravityTimer.updateNextSchedule();
        }
    }

    Timer {
        interval: 24 * 60 * 60 * 1000
        running: true
        repeat: true
        onTriggered: scheduler.usageDatabase.requestPrune("daily-prune-" + Date.now())
    }

    Timer {
        interval: Math.max(5, scheduler.configuration.autoExportIntervalMinutes) * 60 * 1000
        running: scheduler.configuration.autoExportEnabled
                 && scheduler.configuration.autoExportDirectory !== ""
        repeat: true
        onTriggered: {
            var formats = [];
            if (scheduler.configuration.autoExportFormat === "json") {
                formats = ["json"];
            } else if (scheduler.configuration.autoExportFormat === "csv") {
                formats = ["csv"];
            } else {
                formats = ["json", "csv"];
            }
            scheduler.usageDatabase.requestExportAll("scheduled-" + Date.now(),
                                                     scheduler.configuration.autoExportDirectory,
                                                     formats);
        }
    }

    Timer {
        id: copilotTimer
        property date dueAt: new Date(NaN)
        interval: dueAt && isFinite(dueAt.getTime()) ? Math.max(1, dueAt.getTime() - Date.now()) : 1
        running: dueAt && isFinite(dueAt.getTime())
        repeat: false
        function updateNextSchedule() {
            var monitor = scheduler.copilotMonitor;
            if (!scheduler.configuration.copilotEnabled || !monitor
                    || !monitor.githubToken || !monitor.orgName) {
                dueAt = new Date(NaN);
                scheduler.publishSubscriptionDeadline(monitor, dueAt);
                return;
            }
            if (!scheduler.networkPolicy.externalRefreshAllowed || monitor.syncNeedsAction || monitor.syncing) {
                dueAt = new Date(NaN);
                scheduler.publishSubscriptionDeadline(monitor, dueAt);
                return;
            }
            var intervalMs = 60 * 60 * 1000;
            var jitter = scheduler.networkPolicy.deterministicJitterMs("github-copilot");
            var successAt = new Date(monitor.lastSyncTime).getTime();
            var completionAt = new Date(monitor.lastSyncCompletionTime).getTime();
            var base = isFinite(successAt) && successAt > completionAt ? successAt : completionAt;
            var due = isFinite(base) && base > 0
                ? base + intervalMs + jitter : Date.now() + intervalMs + jitter;
            var retry = new Date(monitor.syncRetryAfter).getTime();
            if (!isFinite(retry)) retry = 0;
            dueAt = new Date(Math.max(due, retry));
            scheduler.publishSubscriptionDeadline(monitor, dueAt);
        }
        onTriggered: {
            scheduler.fetchCopilotOrgMetrics(false);
            updateNextSchedule();
        }
        Component.onCompleted: updateNextSchedule()
    }

    Connections {
        target: scheduler.copilotMonitor
        function onOrgMetricsUpdated() { copilotTimer.updateNextSchedule(); }
        function onOrgNameChanged() { copilotTimer.updateNextSchedule(); }
        function onGithubTokenChanged() { copilotTimer.updateNextSchedule(); }
        function onSyncStatusChanged() { copilotTimer.updateNextSchedule(); }
    }
}
