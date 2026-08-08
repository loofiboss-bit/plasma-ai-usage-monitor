import QtQuick

Item {
    id: store

    property var repository: null
    property var policies: []
    property var visiblePolicies: []
    property string selectedKey: ""
    property string filterText: ""
    property bool dirty: false
    property string errorKey: ""
    property string errorDetail: ""
    property string copySuffix: "copy"

    readonly property var selectedPolicy: policyForKey(selectedKey)
    readonly property int selectedIndex: indexForKey(selectedKey)

    signal staged()
    signal applied()
    signal discarded()

    onFilterTextChanged: refreshFilter()
    onRepositoryChanged: reload()

    function clone(value) {
        return value === undefined || value === null
            ? value : JSON.parse(JSON.stringify(value));
    }

    function policyKey(policy) {
        return String(policy?._draftKey || policy?.policyId || "");
    }

    function newUuid() {
        var seed = String(Date.now()) + ":" + String(Math.random());
        var hex = Qt.md5(seed);
        return hex.slice(0, 8) + "-" + hex.slice(8, 12) + "-4"
            + hex.slice(13, 16) + "-8" + hex.slice(17, 20) + "-"
            + hex.slice(20, 32);
    }

    function reload() {
        if (!repository || repository.policies === undefined) return;
        policies = clone(repository.policies || []);
        dirty = false;
        errorKey = "";
        errorDetail = "";
        if (indexForKey(selectedKey) < 0)
            selectedKey = policies.length > 0 ? policyKey(policies[0]) : "";
        refreshFilter();
    }

    function refreshFilter() {
        var needle = filterText.trim().toLocaleLowerCase();
        var result = [];
        for (var i = 0; i < policies.length; ++i) {
            var policy = policies[i] || {};
            var haystack = [policy.sourceId, policy.scopeLabel,
                            policy.scopeKind, policy.periodType,
                            policy.currency].join(" ").toLocaleLowerCase();
            if (needle.length === 0 || haystack.indexOf(needle) >= 0)
                result.push(policy);
        }
        visiblePolicies = result;
    }

    function indexForKey(key) {
        for (var i = 0; i < policies.length; ++i) {
            if (policyKey(policies[i]) === key) return i;
        }
        return -1;
    }

    function policyForKey(key) {
        var index = indexForKey(key);
        return index >= 0 ? policies[index] : ({});
    }

    function select(key) {
        if (indexForKey(key) >= 0) selectedKey = key;
    }

    function markChanged(nextPolicies, preferredKey) {
        policies = nextPolicies;
        dirty = true;
        errorKey = "";
        errorDetail = "";
        if (preferredKey !== undefined) selectedKey = preferredKey;
        refreshFilter();
        staged();
    }

    function setField(field, value) {
        var index = selectedIndex;
        if (index < 0) return;
        var next = clone(policies);
        next[index][field] = value;
        if (field === "scopeMode" && value === "aggregate") {
            next[index].scopeKind = "";
            next[index].scopeIdentity = "";
            next[index].scopeLabel = "";
        }
        if (field === "periodType" && value !== "anchored_month")
            next[index].anchorDay = null;
        markChanged(next, selectedKey);
    }

    function createPolicy(sourceId, periodType) {
        var draft = {
            policyId: newUuid(),
            sourceId: sourceId || "openai",
            sourceKind: "provider",
            scopeMode: "aggregate",
            scopeKind: "",
            scopeIdentity: "",
            scopeLabel: "",
            valueClass: "actual",
            limitMinor: 10000,
            currency: "USD",
            periodType: periodType || "calendar_month",
            anchorDay: null,
            warningPercent: 80,
            criticalPercent: 90,
            notifyEnabled: true,
            enabled: true,
            snoozedUntilUtc: null
        };
        if (repository && typeof repository.validatePolicy === "function") {
            var validation = repository.validatePolicy(draft);
            if (validation.ok && validation.policy)
                draft = clone(validation.policy);
        }
        draft._draftKey = draft.policyId || newUuid();
        var next = clone(policies);
        next.push(draft);
        markChanged(next, policyKey(draft));
    }

    function duplicateSelected() {
        if (selectedIndex < 0) return;
        var duplicate = clone(selectedPolicy);
        duplicate.policyId = newUuid();
        duplicate._draftKey = duplicate.policyId;
        duplicate.createdAtUtc = null;
        duplicate.updatedAtUtc = null;
        duplicate.snoozedUntilUtc = null;
        if (duplicate.scopeLabel)
            duplicate.scopeLabel += " " + copySuffix;
        var next = clone(policies);
        next.push(duplicate);
        markChanged(next, policyKey(duplicate));
    }

    function deleteSelected() {
        var index = selectedIndex;
        if (index < 0) return;
        var next = clone(policies);
        next.splice(index, 1);
        var nextKey = next.length === 0 ? ""
            : policyKey(next[Math.min(index, next.length - 1)]);
        markChanged(next, nextKey);
    }

    function setSnoozedUntilNextPeriod() {
        if (selectedIndex < 0 || !repository
                || typeof repository.nextPeriodStart !== "function") return false;
        var until = repository.nextPeriodStart(selectedPolicy, new Date());
        if (!until || !Number.isFinite(new Date(until).getTime())) {
            errorKey = "snooze-unavailable";
            return false;
        }
        setField("snoozedUntilUtc", until);
        return true;
    }

    function clearSnooze() {
        setField("snoozedUntilUtc", null);
    }

    function cleanPolicy(policy) {
        var result = clone(policy);
        delete result._draftKey;
        return result;
    }

    function apply() {
        if (!repository || typeof repository.replacePolicies !== "function") {
            errorKey = "repository-unavailable";
            return false;
        }
        var payload = [];
        for (var i = 0; i < policies.length; ++i) {
            var candidate = cleanPolicy(policies[i]);
            if (typeof repository.validatePolicy === "function") {
                var validation = repository.validatePolicy(candidate);
                if (!validation.ok) {
                    selectedKey = policyKey(policies[i]);
                    errorKey = "invalid-policy";
                    errorDetail = validation.error || "";
                    return false;
                }
                candidate = validation.policy;
            }
            payload.push(candidate);
        }
        if (!repository.replacePolicies(payload)) {
            errorKey = "save-failed";
            errorDetail = repository.errorString || "";
            return false;
        }
        reload();
        applied();
        return true;
    }

    function discard() {
        reload();
        discarded();
    }

    Component.onCompleted: reload()
    Component.onDestruction: {
        policies = [];
        dirty = false;
    }
}
