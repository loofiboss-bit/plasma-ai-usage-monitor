import QtQuick
import com.github.loofi.aiusagemonitor 1.0

QtObject {
    id: catalog

    readonly property int schemaVersion: ProviderPricingCatalog.schemaVersion
    readonly property string catalogVersion: ProviderPricingCatalog.catalogVersion
    readonly property string lastReviewed: ProviderPricingCatalog.lastReviewed
    readonly property bool runtimeScraping: ProviderPricingCatalog.runtimeScraping
    readonly property string verificationState: ProviderPricingCatalog.verificationState
    readonly property int sequence: ProviderPricingCatalog.sequence
    readonly property string hardExpiresAt: ProviderPricingCatalog.hardExpiresAt
    readonly property bool estimatesAllowed: ProviderPricingCatalog.estimatesAllowed
    readonly property int freshnessSloDays: ProviderPricingCatalog.freshnessSloDays

    // Provider identity, adapter profile, and capabilities come from Catalog v7.
    // Only runtime backend association remains in ProviderRegistry.
    readonly property var providers: {
        var catalogProviders = ProviderPricingCatalog.providers();
        var result = [];
        for (var i = 0; i < catalogProviders.length; i++) {
            var entry = catalogProviders[i];
            var config = entry.config || {};
            result.push({
                displayName: entry.displayName,
                name: entry.displayName,
                label: entry.displayName,
                dbName: entry.dbName,
                configKey: config.key || entry.stableId,
                color: entry.colorToken,
                icon: entry.icon,
                auth: entry.auth,
                credentialSlots: entry.auth?.credentialSlots || [],
                acceptAnyCredentialSet: entry.auth?.acceptAnyCredentialSet || [],
                capabilityCredentialSets: entry.auth?.capabilityCredentialSets || ({}),
                endpoint: entry.endpoint,
                capabilities: entry.capabilities || [],
                expectedSources: entry.expectedSources || [],
                safeRefresh: entry.safeRefresh || {},
                minimumRefreshSeconds: (entry.safeRefresh || {}).minimumIntervalSeconds || 300,
                requestBudget: (entry.safeRefresh || {}).requestBudget || 1,
                monitoringLevel: entry.monitoringLevel || "connectivity_only",
                adapterType: entry.adapterType || "model_discovery",
                probePolicy: entry.probePolicy || "manual_only",
                requiresApiKey: (entry.auth?.credentialSlots || []).length > 0,
                supportsBudget: entry.budgetPolicyContractVersion === "budget-policy-v2",
                budgetPolicyContractVersion: entry.budgetPolicyContractVersion || "",
                supportedBudgetScopes: entry.supportedBudgetScopes || [],
                supportedBillingCycles: entry.supportedBillingCycles || [],
                capabilityReviewedAt: entry.capabilityReviewedAt || "",
                capabilityReviewExpiresAt: entry.capabilityReviewExpiresAt || "",
                enabledConfigKey: config.enabled,
                modelConfigKey: config.model,
                customBaseUrlConfigKey: (config.key || entry.stableId) + "CustomBaseUrl",
                refreshConfigKey: config.refreshInterval,
                notificationsConfigKey: config.notifications,
                secretKey: (entry.auth?.credentialSlots || [])[0] || ""
            });
        }
        return result;
    }

    readonly property var budgetProviders: providers.filter(function(provider) {
        return provider.supportsBudget;
    })

    // Legacy keys are isolated to the one-shot v17 rollback-compatible migration.
    readonly property var legacyBudgetProviders: {
        var catalogProviders = ProviderPricingCatalog.providers();
        var result = [];
        for (var i = 0; i < catalogProviders.length; ++i) {
            var entry = catalogProviders[i];
            var config = entry.config || {};
            if (!config.dailyBudget && !config.monthlyBudget) continue;
            result.push({
                configKey: config.key || entry.stableId,
                dailyBudgetConfigKey: config.dailyBudget || "",
                monthlyBudgetConfigKey: config.monthlyBudget || ""
            });
        }
        return result;
    }
}
