import QtQuick

QtObject {
    id: migration

    required property var configuration
    required property var repository
    required property var catalog

    function migrate() {
        var rows = [];
        var providers = catalog.legacyBudgetProviders || [];
        var warning = Number(configuration.budgetWarningPercent || 80);
        for (var i = 0; i < providers.length; ++i) {
            var provider = providers[i];
            var periods = [
                { key: provider.dailyBudgetConfigKey, type: "calendar_day" },
                { key: provider.monthlyBudgetConfigKey, type: "calendar_month" }
            ];
            for (var periodIndex = 0; periodIndex < periods.length; ++periodIndex) {
                var period = periods[periodIndex];
                if (!period.key) continue;
                rows.push({
                    legacyKey: period.key,
                    sourceId: provider.configKey,
                    sourceKind: "provider",
                    scopeMode: "aggregate",
                    scopeKind: "",
                    scopeIdentity: "",
                    scopeLabel: "",
                    valueClass: "actual",
                    limitMinor: Number(configuration[period.key] || 0),
                    currency: "USD",
                    periodType: period.type,
                    timeZoneId: "UTC",
                    warningPercent: warning,
                    notifyEnabled: true,
                    enabled: true
                });
            }
        }
        return repository.migrateLegacyBudgets(rows);
    }
}
