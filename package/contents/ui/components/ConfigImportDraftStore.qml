import QtQuick

// The configuration window owns this transient draft, never the applet or disk.
Item {
    id: draft
    visible: false
    property var payload: null
    property bool saveFailed: false
    readonly property bool dirty: payload !== null
    signal staged()

    function stage(validatedPayload) {
        payload = JSON.parse(JSON.stringify(validatedPayload));
        saveFailed = false;
        staged();
    }

    function apply(repository, configuration) {
        if (!dirty) return true;
        // replacePolicies is transactional. Do not mutate KConfig before it succeeds.
        if (payload.budgetPolicies !== undefined
                && !repository.replacePolicies(payload.budgetPolicies)) {
            saveFailed = true;
            return false;
        }
        var keys = Object.keys(payload.settings);
        for (var i = 0; i < keys.length; ++i)
            configuration[keys[i]] = payload.settings[keys[i]];
        discard();
        return true;
    }

    function discard() {
        payload = null;
        saveFailed = false;
    }
}
