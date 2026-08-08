import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "BudgetControlUi"
    when: windowShown

    property var policy: ({
        policyId: "11111111-1111-4111-8111-111111111111",
        sourceId: "openai",
        sourceKind: "provider",
        scopeMode: "aggregate",
        scopeKind: "",
        scopeIdentity: "",
        scopeLabel: "A deliberately long translated scope label that must remain readable",
        valueClass: "actual",
        limitMinor: 12345,
        currency: "USD",
        periodType: "calendar_month",
        anchorDay: null,
        timeZoneId: "Europe/Stockholm",
        warningPercent: 80,
        criticalPercent: 90,
        notifyEnabled: true,
        enabled: true,
        snoozedUntilUtc: null
    })

    QtObject {
        id: fakeRepository
        function validatePolicy(candidate) {
            return { ok: true, error: "", policy: candidate };
        }
        function formatMinorAmount(minor, currency) {
            return (Number(minor) / 100).toFixed(2);
        }
        function parseMajorAmount(text, currency) {
            return { ok: true, minor: Math.round(Number(text) * 100), error: "" };
        }
    }

    QtObject {
        id: draftStore
        property var selectedPolicy: testCase.policy
        property var repository: fakeRepository
        function setField(field, value) {
            var next = JSON.parse(JSON.stringify(selectedPolicy));
            next[field] = value;
            selectedPolicy = next;
            testCase.policy = next;
        }
        function setSnoozedUntilNextPeriod() { return true; }
        function clearSnooze() { setField("snoozedUntilUtc", null); }
    }

    QtObject {
        id: catalog
        property var budgetProviders: [{
            configKey: "openai",
            displayName: "OpenAI with a deliberately long translated provider name",
            supportedBudgetScopes: ["aggregate", "project", "line_item"],
            supportedBillingCycles: [
                "calendar_day", "iso_week", "calendar_month", "anchored_month"
            ]
        }]
    }

    Component {
        id: editorComponent
        Components.BudgetPolicyEditor {
            store: draftStore
            providerCatalog: catalog
        }
    }

    function createEditor(width) {
        return createTemporaryObject(editorComponent, testCase, {
            width: width,
            height: 720
        });
    }

    function test_riskPreviewStates() {
        var editor = createEditor(720);
        verify(editor);
        var cases = [
            { percent: 0, label: "Safe" },
            { percent: 80, label: "Warning" },
            { percent: 90, label: "Critical" },
            { percent: 100, label: "Exceeded" },
            { percent: 200, label: "Exceeded" }
        ];
        for (var i = 0; i < cases.length; ++i) {
            editor.previewPercent = cases[i].percent;
            compare(editor.riskLabel(), cases[i].label);
        }
    }

    function test_narrowAndWideRemainAccessible() {
        var narrow = createEditor(300);
        verify(narrow);
        compare(narrow.Accessible.name, "Budget policy editor");
        verify(narrow.contentWidth <= narrow.width + 1);

        var wide = createEditor(920);
        verify(wide);
        compare(wide.Accessible.name, "Budget policy editor");
        verify(wide.contentWidth <= wide.width + 1);
    }

    function test_offscreenRasterIsProduced() {
        var editor = createEditor(640);
        verify(editor);
        var completed = false;
        verify(editor.grabToImage(function(result) {
            completed = true;
        }));
        tryVerify(function() { return completed; }, 5000);
    }
}
