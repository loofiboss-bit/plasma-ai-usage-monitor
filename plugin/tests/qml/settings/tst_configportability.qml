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
}
