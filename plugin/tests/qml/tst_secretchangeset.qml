import QtQuick
import QtTest
import "../../../package/contents/ui" as AppUi

TestCase {
    id: testCase
    name: "SecretChangeSet"

    QtObject {
        id: fakeStore

        property bool walletOpen: true
        property var storeCalls: []
        property var removeCalls: []
        property var rejectedKeys: []

        function reset() {
            walletOpen = true;
            storeCalls = [];
            removeCalls = [];
            rejectedKeys = [];
        }

        function storeKey(key, value) {
            storeCalls = storeCalls.concat([{ key: key, value: value }]);
            return rejectedKeys.indexOf(key) < 0;
        }

        function removeKey(key) {
            removeCalls = removeCalls.concat([key]);
            return rejectedKeys.indexOf(key) < 0;
        }
    }

    AppUi.SecretChangeSet {
        id: changeSet
        store: fakeStore
    }

    Component {
        id: changeSetComponent
        AppUi.SecretChangeSet {}
    }

    function init() {
        fakeStore.reset();
        changeSet.discard();
    }

    function test_destructionDoesNotCommit() {
        var temporary = changeSetComponent.createObject(testCase, { store: fakeStore });
        verify(temporary !== null);
        temporary.stageStore("openai", "replacement");
        verify(temporary.dirty);
        temporary.destroy();

        compare(fakeStore.storeCalls.length, 0);
        compare(fakeStore.removeCalls.length, 0);
    }

    function test_discardDoesNotCommit() {
        changeSet.stageRemove("openai");
        changeSet.discard();

        verify(!changeSet.dirty);
        compare(fakeStore.storeCalls.length, 0);
        compare(fakeStore.removeCalls.length, 0);
    }

    function test_storeAndRepeatedCommit() {
        changeSet.stageStore("openai", "replacement");
        var first = changeSet.commit();

        verify(first.ok);
        compare(first.appliedKeys, ["openai"]);
        compare(fakeStore.storeCalls.length, 1);
        compare(fakeStore.storeCalls[0].value, "replacement");
        verify(!changeSet.dirty);

        var second = changeSet.commit();
        verify(second.ok);
        compare(fakeStore.storeCalls.length, 1);
    }

    function test_untouchedMaskedSecretIsNotStaged() {
        changeSet.stageStore("openai", "********");

        verify(!changeSet.dirty);
        var result = changeSet.commit();
        verify(result.ok);
        compare(fakeStore.storeCalls.length, 0);
        compare(fakeStore.removeCalls.length, 0);
    }

    function test_remove() {
        changeSet.stageRemove("openai");
        var result = changeSet.commit();

        verify(result.ok);
        compare(result.appliedKeys, ["openai"]);
        compare(fakeStore.removeCalls, ["openai"]);
    }

    function test_multipleChanges() {
        changeSet.stageStore("openai", "one");
        changeSet.stageStore("anthropic", "two");
        changeSet.stageRemove("google");
        var result = changeSet.commit();

        verify(result.ok);
        compare(result.appliedKeys.length, 3);
        compare(fakeStore.storeCalls.length, 2);
        compare(fakeStore.removeCalls, ["google"]);
    }

    function test_failureRemainsStaged() {
        fakeStore.rejectedKeys = ["anthropic"];
        changeSet.stageStore("openai", "one");
        changeSet.stageStore("anthropic", "two");
        var first = changeSet.commit();

        verify(!first.ok);
        compare(first.appliedKeys, ["openai"]);
        compare(first.failedKeys, ["anthropic"]);
        verify(changeSet.dirty);

        fakeStore.rejectedKeys = [];
        var second = changeSet.commit();
        verify(second.ok);
        compare(fakeStore.storeCalls.length, 3);
        compare(fakeStore.storeCalls[2].key, "anthropic");
        verify(!changeSet.dirty);
    }

    function test_closedWalletDoesNotCallStore() {
        fakeStore.walletOpen = false;
        changeSet.stageStore("openai", "replacement");
        var result = changeSet.commit();

        verify(!result.ok);
        compare(result.message, "wallet-not-open");
        compare(fakeStore.storeCalls.length, 0);
        verify(changeSet.dirty);
    }
}
