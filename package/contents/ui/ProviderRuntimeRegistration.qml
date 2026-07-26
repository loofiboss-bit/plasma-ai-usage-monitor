import QtQuick

QtObject {
    id: registration

    required property var configuration
    required property var providerManager
    required property var providerRegistry
    required property var sourceReadinessModel
    required property var dailyStateModel
    required property var nativeBackends

    property bool initialized: false

    function hasNativeBackend(configKey) {
        for (var i = 0; i < nativeBackends.length; ++i) {
            if (nativeBackends[i].configKey === configKey)
                return true;
        }
        return false;
    }

    function configureDescriptorBackend(configKey) {
        var backend = providerManager.backend(configKey);
        if (!backend) return;
        var modelKey = configKey + "Model";
        var urlKey = configKey + "CustomBaseUrl";
        // ProviderManager intentionally returns heterogeneous backend types.
        // qmllint disable missing-property
        if (backend["model"] !== undefined)
            backend["model"] = configuration[modelKey] || "";
        // qmllint enable missing-property
        backend.customBaseUrl = configuration[urlKey] || "";
        backend.dailyBudget = (configuration[configKey + "DailyBudget"] || 0) / 100.0;
        backend.monthlyBudget = (configuration[configKey + "MonthlyBudget"] || 0) / 100.0;
        backend.budgetWarningPercent = configuration.budgetWarningPercent;
    }

    function initialize() {
        if (initialized) return;

        for (var i = 0; i < nativeBackends.length; ++i) {
            var nativeBackend = nativeBackends[i];
            providerManager.registerBackend(nativeBackend.configKey,
                                            nativeBackend.backend);
        }

        var catalogProviders = providerRegistry.providerCatalog.providers || [];
        for (var j = 0; j < catalogProviders.length; ++j) {
            var descriptor = catalogProviders[j];
            if (!providerManager.backend(descriptor.configKey))
                continue;
            if (!hasNativeBackend(descriptor.configKey))
                configureDescriptorBackend(descriptor.configKey);
        }

        dailyStateModel.registerReadinessModel(sourceReadinessModel);
        var providers = providerRegistry.allProviders || [];
        for (var k = 0; k < providers.length; ++k) {
            var provider = providers[k];
            sourceReadinessModel.registerProviderBackend(provider.configKey,
                                                         provider.backend);
            sourceReadinessModel.setSourceEnabled(provider.configKey,
                                                  provider.enabled);
            dailyStateModel.registerProviderBackend(provider.configKey,
                                                    provider.backend);
            dailyStateModel.setHistoryIdentity(provider.configKey,
                                               provider.dbName);
        }

        var tools = providerRegistry.allSubscriptionTools || [];
        for (var n = 0; n < tools.length; ++n) {
            var tool = tools[n];
            sourceReadinessModel.registerLocalTool(tool.stableId, tool.monitor);
            dailyStateModel.registerLocalTool(tool.stableId, tool.monitor);
            dailyStateModel.setHistoryIdentity(tool.stableId, tool.name);
        }
        initialized = true;
    }
}
