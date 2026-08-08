import QtQuick

QtObject {
    id: runtime

    required property var repository
    required property var registry

    readonly property int revision: repository ? repository.revision : 0

    function queryPolicies() {
        var policies = [];
        var rows = repository ? repository.policies : [];
        for (var index = 0; index < rows.length; ++index) {
            var policy = Object.assign({}, rows[index]);
            if (!policy.enabled) continue;
            var provider = registry.providerByConfigKey(policy.sourceId);
            policy.provider = provider ? provider.dbName : policy.sourceId;
            policy.observationScope = "organization";
            policy.budgetPolicyContractVersion = provider
                ? provider.budgetPolicyContractVersion : "";
            policy.catalogSupportedScopes = provider
                ? provider.supportedBudgetScopes : [];
            policy.catalogSupportedBillingCycles = provider
                ? provider.supportedBillingCycles : [];
            policy.catalogSupportsProviderReset = policy.catalogSupportedBillingCycles
                .indexOf("provider_reset") >= 0;
            policy.validatedGatewayScopes = provider && provider.backend
                && provider.backend["validatedBudgetScopes"] !== undefined
                ? provider.backend["validatedBudgetScopes"] : [];
            policies.push(policy);
        }
        return policies;
    }
}
