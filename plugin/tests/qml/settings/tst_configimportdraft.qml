import QtQuick
import QtQuick.Window
import QtTest
import "../../../../package/contents/ui/components" as Components
import "../../../../package/contents/ui/ConfigPortability.js" as ConfigPortability

TestCase {
    name: "ConfigImportDraft"
    QtObject {
        id: repository
        property bool rejectWrites: false
        property var policies: [{ policyId: "original" }]
        property int writeCalls: 0
        function replacePolicies(value) {
            writeCalls++;
            if (rejectWrites) return false;
            policies = JSON.parse(JSON.stringify(value));
            return true;
        }
    }
    QtObject {
        id: configuration
        property int refreshInterval: 300
    }
    Components.ConfigImportDraftStore { id: draft }
    Item { id: sessionHost }
    Component {
        id: pageFactory
        Item {
            property var currentDraft: null
            property bool retainDraftOnDeparture: false
            function createDraft(host) {
                currentDraft = ConfigPortability.createImportDraft(host, "sessionDraft");
                return currentDraft;
            }
            Component.onDestruction: {
                if (currentDraft && !retainDraftOnDeparture) currentDraft.destroy();
            }
        }
    }
    Window {
        id: configWindow
        width: 320
        height: 200
        Components.ConfigApplyGuard { id: applyGuard; window: configWindow }
        function nativeOKIgnoringSaveResult() {
            var result = draft.apply(repository, configuration);
            if (!result) applyGuard.protectFailedApply();
            close();
        }
    }
    function init() {
        configWindow.hide();
        applyGuard.failedApplyInProgress = false;
        draft.discard();
        repository.rejectWrites = false;
        repository.policies = [{ policyId: "original" }];
        repository.writeCalls = 0;
        configuration.refreshInterval = 300;
    }
    function payload() {
        return { ok: true, settings: { refreshInterval: 600 }, budgetPolicies: [{ policyId: "imported" }] };
    }
    function test_selectionAndCancelDoNotWrite() {
        var value = payload();
        draft.stage(value);
        value.settings.refreshInterval = 999;
        verify(draft.dirty);
        compare(repository.writeCalls, 0);
        compare(configuration.refreshInterval, 300);
        compare(draft.payload.settings.refreshInterval, 600);
        draft.discard();
        compare(repository.policies[0].policyId, "original");
        compare(configuration.refreshInterval, 300);
    }
    function test_transactionFailurePreservesBothStoresAndPendingDraft() {
        draft.stage(payload());
        repository.rejectWrites = true;
        verify(!draft.apply(repository, configuration));
        compare(repository.policies[0].policyId, "original");
        compare(configuration.refreshInterval, 300);
        verify(draft.dirty);
        verify(draft.saveFailed);
        repository.rejectWrites = false;
        verify(draft.apply(repository, configuration));
        compare(configuration.refreshInterval, 600);
        compare(repository.policies[0].policyId, "imported");
        verify(!draft.dirty);
    }
    function test_legacyImportKeepsPolicies() {
        draft.stage({ settings: { refreshInterval: 900 } });
        verify(draft.apply(repository, configuration));
        compare(configuration.refreshInterval, 900);
        compare(repository.writeCalls, 0);
        compare(repository.policies[0].policyId, "original");
    }
    function test_nativeDiscardReleasesPreparedDraftWithoutSaving() {
        var page = pageFactory.createObject(sessionHost);
        var pending = page.createDraft(sessionHost);
        pending.stage(payload());
        // Cancelling the category prompt leaves the current page and draft intact.
        verify(pending.dirty);
        // Native Discard opens the selected category and destroys this page.
        page.destroy();
        wait(0);
        wait(0);
        for (var i = 0; i < sessionHost.children.length; ++i)
            verify(sessionHost.children[i].objectName !== "sessionDraft");
        compare(repository.writeCalls, 0);
        compare(configuration.refreshInterval, 300);
    }

    function test_failedDraftSurvivesPageReplacementWithinWindow() {
        var page = pageFactory.createObject(sessionHost);
        var pending = page.createDraft(sessionHost);
        pending.stage(payload());
        repository.rejectWrites = true;
        page.retainDraftOnDeparture = !pending.apply(repository, configuration);
        verify(page.retainDraftOnDeparture);
        page.destroy();
        wait(0);
        verify(pending.dirty);
        compare(pending.payload.settings.refreshInterval, 600);
        repository.rejectWrites = false;
        verify(pending.apply(repository, configuration));
        compare(configuration.refreshInterval, 600);
        pending.destroy();
    }

    function test_failedNativeOKKeepsWindowAndDraftThenCancelCloses() {
        draft.stage(payload());
        repository.rejectWrites = true;
        configWindow.show();
        tryCompare(configWindow, "visible", true);
        configWindow.nativeOKIgnoringSaveResult();
        verify(configWindow.visible);
        verify(draft.dirty);
        compare(configuration.refreshInterval, 300);
        compare(repository.policies[0].policyId, "original");
        tryCompare(applyGuard, "failedApplyInProgress", false);
        configWindow.close();
        tryCompare(configWindow, "visible", false);
        compare(configuration.refreshInterval, 300);
        compare(repository.policies[0].policyId, "original");
    }

    function test_successfulNativeOKClosesAndSaves() {
        draft.stage(payload());
        configWindow.show();
        tryCompare(configWindow, "visible", true);
        configWindow.nativeOKIgnoringSaveResult();
        tryCompare(configWindow, "visible", false);
        verify(!draft.dirty);
        compare(configuration.refreshInterval, 600);
        compare(repository.policies[0].policyId, "imported");
    }

}
