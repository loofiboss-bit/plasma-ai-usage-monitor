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

    readonly property int refreshStartup: 0
    readonly property int refreshScheduled: 1
    readonly property int refreshPopupOpened: 2
    readonly property int refreshManual: 3
    readonly property int refreshConfigurationChanged: 4
    readonly property int refreshCredentialChanged: 5

    RefreshSchedulerModel { id: refreshPolicy }

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

    function nextSchedule(provider, base) {
        return refreshPolicy.nextScheduledRefresh(
            base, provider.configKey || "", constrainedProviderInterval(provider),
            configuration.refreshInterval || 60, popupOpen,
            provider.backend?.consecutiveErrors || 0,
            provider.backend?.retryable || false);
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
        if (!force && isFresh(provider)) {
            return;
        }
        if (canRefreshBackend(provider.backend, provider.requiresApiKey !== false)) {
            provider.backend.requestRefresh(reason === undefined ? refreshManual : reason);
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

        if (configuration.claudeCodeEnabled && claudeCodeMonitor.installed) {
            browserSyncService.sync("claude", claudeCodeMonitor);
        }

        if (configuration.codexEnabled && codexCliMonitor.installed) {
            browserSyncService.sync("codex", codexCliMonitor);
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

    Instantiator {
        model: scheduler.registry.allProviders

        delegate: Timer {
            required property var modelData

            interval: scheduler.scheduledInterval(modelData)
            running: modelData.enabled
            repeat: true
            onTriggered: scheduler.refreshProvider(modelData, scheduler.refreshScheduled, true)
            onIntervalChanged: updateNextSchedule()

            function updateNextSchedule() {
                if (!modelData.backend) return;
                var base = modelData.backend.lastSuccess || new Date();
                modelData.backend.setNextScheduledRefresh(scheduler.nextSchedule(modelData, base));
            }

            Component.onCompleted: updateNextSchedule()
        }
    }

    Timer {
        interval: Math.max(60, scheduler.configuration.browserSyncInterval) * 1000
        running: scheduler.configuration.browserSyncEnabled
        repeat: true
        onTriggered: scheduler.performBrowserSync()
    }

    Timer {
        interval: Math.max(60, scheduler.configuration.antigravityRefreshInterval || 300) * 1000
        running: scheduler.configuration.antigravityEnabled
        repeat: true
        onTriggered: scheduler.refreshAntigravity(true)
    }

    Timer {
        interval: 24 * 60 * 60 * 1000
        running: true
        repeat: true
        onTriggered: scheduler.usageDatabase.pruneOldData()
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
        interval: 60 * 60 * 1000
        running: scheduler.configuration.copilotEnabled
                 && scheduler.copilotMonitor.githubToken !== ""
                 && scheduler.copilotMonitor.orgName !== ""
        repeat: true
        onTriggered: scheduler.copilotMonitor.fetchOrgMetrics()
    }
}
