import QtQml

QtObject {
    id: changeSet

    required property var store

    property var pendingChanges: ({})
    readonly property bool dirty: Object.keys(pendingChanges).length > 0
    property var lastResult: ({
        ok: true,
        appliedKeys: [],
        failedKeys: [],
        message: ""
    })

    function stageStore(key, value) {
        if (!key || value === "********") {
            unstage(key);
            return;
        }

        var next = Object.assign({}, pendingChanges);
        next[key] = { action: "store", value: value };
        pendingChanges = next;
        clearResult();
    }

    function stageRemove(key) {
        if (!key) {
            return;
        }

        var next = Object.assign({}, pendingChanges);
        next[key] = { action: "remove", value: "" };
        pendingChanges = next;
        clearResult();
    }

    function unstage(key) {
        if (!key || pendingChanges[key] === undefined) {
            return;
        }

        var next = Object.assign({}, pendingChanges);
        delete next[key];
        pendingChanges = next;
    }

    function discard() {
        pendingChanges = {};
        clearResult();
    }

    function clearResult() {
        lastResult = {
            ok: true,
            appliedKeys: [],
            failedKeys: [],
            message: ""
        };
    }

    function commit() {
        var keys = Object.keys(pendingChanges);
        if (keys.length === 0) {
            clearResult();
            return lastResult;
        }

        if (!store || !store.walletOpen) {
            lastResult = {
                ok: false,
                appliedKeys: [],
                failedKeys: keys,
                message: "wallet-not-open"
            };
            return lastResult;
        }

        var remaining = Object.assign({}, pendingChanges);
        var applied = [];
        var failed = [];

        for (var i = 0; i < keys.length; ++i) {
            var key = keys[i];
            var change = pendingChanges[key];
            var ok = change.action === "remove"
                ? store.removeKey(key)
                : store.storeKey(key, change.value);
            if (ok) {
                applied.push(key);
                delete remaining[key];
            } else {
                failed.push(key);
            }
        }

        pendingChanges = remaining;
        lastResult = {
            ok: failed.length === 0,
            appliedKeys: applied,
            failedKeys: failed,
            message: failed.length === 0 ? "saved" : "write-failed"
        };
        return lastResult;
    }
}
