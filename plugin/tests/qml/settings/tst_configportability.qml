import QtQuick
import QtTest
import "../../../../package/contents/ui/ConfigPortability.js" as ConfigPortability

TestCase {
    name: "ConfigPortability"

    readonly property var activeKeys: [
        "refreshInterval", "compactDisplayMode", "openaiEnabled"
    ]

    function test_schemaV2RetiredKeysAreIgnored() {
        var payload = {
            schemaVersion: 2,
            settings: {
                refreshInterval: 600,
                compactDisplayMode: "attention",
                openaiEnabled: true,
                dashboardMode: "analyst",
                showOnlyProblems: true,
                unknownFutureKey: "ignored"
            }
        };

        var filtered = ConfigPortability.schemaV2Settings(payload, activeKeys);

        compare(Object.keys(filtered).length, 3);
        compare(filtered.refreshInterval, 600);
        compare(filtered.compactDisplayMode, "attention");
        compare(filtered.openaiEnabled, true);
        verify(filtered.dashboardMode === undefined);
        verify(filtered.showOnlyProblems === undefined);
        verify(filtered.unknownFutureKey === undefined);
    }

    function test_nonSchemaV2PayloadIsNotApplied() {
        compare(Object.keys(ConfigPortability.schemaV2Settings({
            schemaVersion: 1,
            settings: { refreshInterval: 600 }
        }, activeKeys)).length, 0);
    }

    function validPolicy() {
        return {
            policyId: "11111111-1111-4111-8111-111111111111",
            sourceId: "openai", sourceKind: "provider",
            scopeMode: "aggregate", scopeKind: "", scopeIdentity: "", scopeLabel: "",
            valueClass: "actual", limitMinor: 10000, currency: "USD",
            periodType: "calendar_month", anchorDay: null,
            timeZoneId: "Europe/Stockholm", warningPercent: 80,
            criticalPercent: 90, notifyEnabled: true, enabled: true
        };
    }

    function test_schemaV3FullRestoreIsValidated() {
        var payload = ConfigPortability.schemaV3Payload({
            schemaVersion: 3,
            settings: { refreshInterval: 600, openaiEnabled: true },
            budgetPolicies: [validPolicy()]
        }, activeKeys, { refreshInterval: 300, compactDisplayMode: "icon", openaiEnabled: false });
        verify(payload.ok);
        compare(payload.settings.refreshInterval, 600);
        compare(payload.budgetPolicies.length, 1);
    }

    function test_schemaV3RejectsBrokenAndPartialShape() {
        var missingPolicies = ConfigPortability.schemaV3Payload({
            schemaVersion: 3, settings: { refreshInterval: 600 }
        }, activeKeys, { refreshInterval: 300 });
        verify(!missingPolicies.ok);

        var policy = validPolicy();
        policy.limitMinor = 0;
        var invalidPolicy = ConfigPortability.schemaV3Payload({
            schemaVersion: 3, settings: {}, budgetPolicies: [policy]
        }, activeKeys, {});
        verify(!invalidPolicy.ok);

        var unknownSetting = ConfigPortability.schemaV3Payload({
            schemaVersion: 3, settings: { futureKey: true }, budgetPolicies: []
        }, activeKeys, {});
        verify(!unknownSetting.ok);
    }
}
