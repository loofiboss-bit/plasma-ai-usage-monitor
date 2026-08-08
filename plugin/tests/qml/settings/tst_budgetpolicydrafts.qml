import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "BudgetPolicyDraftStore"

    property var baselinePolicies: [{
        policyId: "11111111-1111-4111-8111-111111111111",
        sourceId: "openai",
        sourceKind: "provider",
        scopeMode: "aggregate",
        scopeKind: "",
        scopeIdentity: "",
        scopeLabel: "",
        valueClass: "actual",
        limitMinor: 10000,
        currency: "USD",
        periodType: "calendar_month",
        anchorDay: null,
        timeZoneId: "Europe/Stockholm",
        warningPercent: 80,
        criticalPercent: 90,
        notifyEnabled: true,
        enabled: true,
        snoozedUntilUtc: null
    }]

    QtObject {
        id: repository
        property var policies: []
        property string errorString: ""
        property int replaceCalls: 0
        property bool failReplace: false

        function validatePolicy(policy) {
            var candidate = JSON.parse(JSON.stringify(policy));
            if (!candidate.timeZoneId) candidate.timeZoneId = "UTC";
            if (Number(candidate.limitMinor) <= 0)
                return { ok: false, error: "limitMinor must be positive" };
            if (Number(candidate.warningPercent) <= 0
                    || Number(candidate.warningPercent)
                       > Number(candidate.criticalPercent)
                    || Number(candidate.criticalPercent) > 100)
                return { ok: false, error: "invalid thresholds" };
            if (candidate.scopeMode === "scoped"
                    && (!candidate.scopeKind || !candidate.scopeIdentity))
                return { ok: false, error: "scope identity required" };
            return { ok: true, error: "", policy: candidate };
        }

        function replacePolicies(nextPolicies) {
            replaceCalls += 1;
            if (failReplace) {
                errorString = "injected save failure";
                return false;
            }
            policies = JSON.parse(JSON.stringify(nextPolicies));
            return true;
        }

        function nextPeriodStart(policy, generatedAt) {
            return "2026-09-01T00:00:00.000Z";
        }
    }

    Components.BudgetPolicyDraftStore {
        id: drafts
        repository: repository
        copySuffix: "copy"
    }

    function clone(value) {
        return JSON.parse(JSON.stringify(value));
    }

    function init() {
        repository.replaceCalls = 0;
        repository.failReplace = false;
        repository.errorString = "";
        repository.policies = clone(testCase.baselinePolicies);
        drafts.reload();
    }

    function test_createDuplicateEnableDeleteStayStaged() {
        compare(drafts.policies.length, 1);
        drafts.createPolicy("anthropic", "iso_week");
        compare(drafts.policies.length, 2);
        verify(drafts.dirty);
        compare(repository.replaceCalls, 0);

        var createdId = drafts.selectedPolicy.policyId;
        verify(createdId.length > 0);
        drafts.setField("enabled", false);
        verify(!drafts.selectedPolicy.enabled);
        drafts.duplicateSelected();
        compare(drafts.policies.length, 3);
        verify(drafts.selectedPolicy.policyId !== createdId);
        drafts.deleteSelected();
        compare(drafts.policies.length, 2);
        compare(repository.replaceCalls, 0);
    }

    function test_applyValidatesThenReplacesExactlyOnce() {
        drafts.setField("limitMinor", 12500);
        verify(drafts.apply());
        compare(repository.replaceCalls, 1);
        compare(repository.policies[0].limitMinor, 12500);
        verify(repository.policies[0]._draftKey === undefined);
        verify(!drafts.dirty);
    }

    function test_invalidAndFailedApplyPreserveStagedState() {
        drafts.setField("warningPercent", 95);
        drafts.setField("criticalPercent", 90);
        verify(!drafts.apply());
        compare(repository.replaceCalls, 0);
        verify(drafts.dirty);
        compare(drafts.errorKey, "invalid-policy");

        drafts.setField("warningPercent", 80);
        repository.failReplace = true;
        verify(!drafts.apply());
        compare(repository.replaceCalls, 1);
        verify(drafts.dirty);
        compare(drafts.errorKey, "save-failed");
        compare(drafts.selectedPolicy.warningPercent, 80);
    }

    function test_cancelRestoresRepositorySnapshot() {
        drafts.setField("limitMinor", 42);
        drafts.createPolicy("anthropic", "calendar_day");
        verify(drafts.dirty);
        drafts.discard();
        verify(!drafts.dirty);
        compare(drafts.policies.length, 1);
        compare(drafts.policies[0].limitMinor, 10000);
        compare(repository.replaceCalls, 0);
    }

    function test_scopeResetSearchAndSnooze() {
        drafts.setField("scopeMode", "scoped");
        drafts.setField("scopeKind", "project");
        drafts.setField("scopeIdentity", "project-local-only");
        drafts.setField("scopeLabel", "Research");
        drafts.filterText = "research";
        compare(drafts.visiblePolicies.length, 1);
        verify(drafts.setSnoozedUntilNextPeriod());
        compare(drafts.selectedPolicy.snoozedUntilUtc,
                "2026-09-01T00:00:00.000Z");

        drafts.setField("scopeMode", "aggregate");
        compare(drafts.selectedPolicy.scopeKind, "");
        compare(drafts.selectedPolicy.scopeIdentity, "");
        compare(drafts.selectedPolicy.scopeLabel, "");
    }
}
