import QtQuick

// One runtime cache shared by Overview and panel. The existing daily clock drives rollover.
QtObject {
    id: state
    property var sessionModel: null
    property var presentationTime: new Date()
    property var summaryByTool: ({})
    property var pending: ({})
    property int generation: 0
    readonly property string requestPrefix: Date.now().toString(36) + Math.random().toString(36).slice(2)
    readonly property string dayKey: new Date(presentationTime).toLocaleDateString()
    readonly property var enabledToolIds: sessionModel ? [
        sessionModel.codexEnabled ? "codex-cli" : "",
        sessionModel.claudeCodeEnabled ? "claude-code" : ""
    ].filter(function(id) { return id !== ""; }) : []
    onEnabledToolIdsChanged: refresh()
    onDayKeyChanged: refresh()
    onSessionModelChanged: refresh()
    onPresentationTimeChanged: {
        var tools = Object.keys(summaryByTool);
        if (tools.some(function(id) { return !!summaryByTool[id].queryFailed; })) refresh();
    }

    function refresh() {
        pending = ({});
        summaryByTool = ({});
        generation++;
        if (!sessionModel || typeof sessionModel.requestActivitySummary !== "function") return;
        var now = new Date();
        var start = new Date(now.getTime());
        start.setHours(0, 0, 0, 0);
        var next = {};
        for (var i = 0; i < enabledToolIds.length; ++i) {
            var id = "activity-" + requestPrefix + "-" + generation + "-" + i;
            next[id] = enabledToolIds[i];
        }
        pending = next;
        var ids = Object.keys(next);
        for (var j = 0; j < ids.length; ++j)
            sessionModel.requestActivitySummary(ids[j], next[ids[j]], start.toISOString(), now.toISOString());
    }
    function accept(requestId, summary) {
        var tool = pending[requestId];
        if (!tool || enabledToolIds.indexOf(tool) < 0) return;
        var requests = Object.assign({}, pending);
        delete requests[requestId];
        pending = requests;
        var next = Object.assign({}, summaryByTool);
        next[tool] = summary;
        summaryByTool = next;
    }
    function activityText(toolId) {
        if (enabledToolIds.indexOf(toolId) < 0) return "";
        var data = summaryByTool[toolId];
        if (!data) return i18n("Loading local activity…");
        if (data.queryFailed) return i18n("Local activity unavailable");
        if (data.coverage === "unavailable" || !Number(data.eventCount || 0))
            return i18n("No recorded local observations today");
        var text = i18np("%1 session today", "%1 sessions today", Number(data.sessionCount || 0));
        if (data.coverage && data.coverage !== "reported") text += i18n(" · partial coverage");
        return text;
    }
    function scopedTotals(sourceIds) {
        var tools = sourceIds === null ? enabledToolIds : enabledToolIds.filter(function(id) {
            return sourceIds.indexOf(id) >= 0;
        });
        if (!tools.length) return {available: false};
        var count = 0;
        var tokens = 0;
        var tokensAvailable = true;
        var partial = false;
        for (var i = 0; i < tools.length; ++i) {
            var data = summaryByTool[tools[i]];
            if (!data || data.queryFailed || data.coverage === "unavailable"
                    || !Number(data.eventCount || 0)) return {available: false};
            count += Number(data.sessionCount || 0);
            if (typeof data.totalTokens !== "number" || !Number.isFinite(data.totalTokens))
                tokensAvailable = false;
            else tokens += data.totalTokens;
            var tokenCoverage = (data.categoryCoverage || {}).totalTokens || {};
            if (data.coverage !== "reported"
                    || tokenCoverage.coverage && tokenCoverage.coverage !== "reported") partial = true;
        }
        return {available: true, sessionCount: count, totalTokens: tokensAvailable ? tokens : null,
            partial: partial};
    }
    property Connections backendConnections: Connections {
        target: state.sessionModel
        ignoreUnknownSignals: true
        function onActivitySummaryReady(requestId, summary) { state.accept(requestId, summary); }
        function onQueryFailed(requestId, errorKey) { state.accept(requestId, {queryFailed: true}); }
        function onImportFinished() { state.refresh(); }
        function onClearFinished() { state.refresh(); }
    }
    Component.onCompleted: refresh()
}
