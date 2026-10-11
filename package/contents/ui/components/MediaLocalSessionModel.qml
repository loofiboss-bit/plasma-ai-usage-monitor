import QtQuick

QtObject {
    id: mediaModel

    property string toolId: "codex-cli"
    property bool codexEnabled: true
    property bool claudeCodeEnabled: true
    property int retentionDays: 30
    readonly property string status: "ready"
    readonly property int pendingWorkerCount: 0
    property var aliases: ({
        "project-plasma": "Plasma panel",
        "project-localization": "Localization"
    })

    signal sessionsReady(string requestId, var rows)
    signal summaryReady(string requestId, var summary)
    signal dailyReady(string requestId, var rows)
    signal projectsReady(string requestId, var rows)
    signal exportReady(string requestId, string errorKey)
    signal clearFinished(string requestId, bool ok, string errorKey)
    signal importFinished(string toolId, int count, bool partial, string errorKey)

    function timeAgo(days, hours) {
        var date = new Date();
        date.setHours(date.getHours() - Number(hours || 0));
        date.setDate(date.getDate() - Number(days || 0));
        return date.toISOString();
    }

    function fixtureRows(requestedTool) {
        var isClaude = requestedTool === "claude-code";
        var firstModel = isClaude ? "claude-sonnet" : "gpt-5-codex";
        var secondModel = isClaude ? "claude-opus" : "gpt-5-codex-mini";
        return [
            {
                sessionId: "session-local-01",
                projectId: "project-plasma",
                startedAtUtc: timeAgo(0, 3),
                lastSeenAtUtc: timeAgo(0, 1),
                coverage: "complete",
                eventCount: 4,
                inputTokens: 18420,
                outputTokens: 3920,
                cacheReadTokens: 8410,
                cacheWriteTokens: 1260,
                modelBreakdown: [
                    { modelId: firstModel, eventCount: 3, inputTokens: 14000,
                        outputTokens: 3000, cacheReadTokens: 7000,
                        cacheWriteTokens: 1000 },
                    { modelId: secondModel, eventCount: 1, inputTokens: 4420,
                        outputTokens: 920, cacheReadTokens: 1410,
                        cacheWriteTokens: 260 }
                ]
            },
            {
                sessionId: "session-local-02",
                projectId: "project-localization",
                startedAtUtc: timeAgo(1, 6),
                lastSeenAtUtc: timeAgo(1, 4),
                coverage: "reported",
                eventCount: 3,
                inputTokens: 9320,
                outputTokens: 2110,
                cacheReadTokens: 3200,
                cacheWriteTokens: 480,
                modelBreakdown: [{ modelId: firstModel, eventCount: 3,
                    inputTokens: 9320, outputTokens: 2110,
                    cacheReadTokens: 3200, cacheWriteTokens: 480 }]
            },
            {
                sessionId: "session-local-03",
                projectId: "project-plasma",
                startedAtUtc: timeAgo(4, 8),
                lastSeenAtUtc: timeAgo(4, 6),
                coverage: "partial",
                eventCount: 2,
                inputTokens: 6240,
                outputTokens: 1480,
                cacheReadTokens: 1720,
                cacheWriteTokens: 220,
                modelBreakdown: [{ modelId: secondModel, eventCount: 2,
                    inputTokens: 6240, outputTokens: 1480,
                    cacheReadTokens: 1720, cacheWriteTokens: 220 }]
            },
            {
                sessionId: "session-local-04",
                projectId: "project-localization",
                startedAtUtc: timeAgo(18, 5),
                lastSeenAtUtc: timeAgo(18, 3),
                coverage: "reported",
                eventCount: 1,
                inputTokens: 2380,
                outputTokens: 620,
                cacheReadTokens: 0,
                cacheWriteTokens: 0,
                modelBreakdown: [{ modelId: firstModel, eventCount: 1,
                    inputTokens: 2380, outputTokens: 620,
                    cacheReadTokens: 0, cacheWriteTokens: 0 }]
            }
        ];
    }

    function filteredRows(requestedTool, fromUtc, toUtc, projectId) {
        var from = Date.parse(fromUtc || "") || 0;
        var to = Date.parse(toUtc || "") || Number.MAX_SAFE_INTEGER;
        var rows = fixtureRows(requestedTool);
        var filtered = [];
        for (var i = 0; i < rows.length; ++i) {
            var timestamp = Date.parse(rows[i].lastSeenAtUtc);
            if (timestamp < from || timestamp > to) continue;
            if (projectId && rows[i].projectId !== projectId) continue;
            filtered.push(rows[i]);
        }
        return filtered;
    }

    function modelSummary(rows, requestedTool) {
        var totals = ({});
        var isClaude = requestedTool === "claude-code";
        var names = isClaude
            ? ["claude-sonnet", "claude-opus"] : ["gpt-5-codex", "gpt-5-codex-mini"];
        for (var i = 0; i < rows.length; ++i) {
            var models = rows[i].modelBreakdown || [];
            for (var j = 0; j < models.length; ++j) {
                var name = models[j].modelId;
                if (!totals[name]) totals[name] = {
                    modelId: name, eventCount: 0, inputTokens: 0,
                    outputTokens: 0, cacheReadTokens: 0, cacheWriteTokens: 0
                };
                totals[name].eventCount += models[j].eventCount;
                totals[name].inputTokens += Number(models[j].inputTokens || 0);
                totals[name].outputTokens += Number(models[j].outputTokens || 0);
                totals[name].cacheReadTokens += Number(models[j].cacheReadTokens || 0);
                totals[name].cacheWriteTokens += Number(models[j].cacheWriteTokens || 0);
            }
        }
        return names.filter(function(name) { return !!totals[name]; })
            .map(function(name) { return totals[name]; });
    }

    function summaryFor(rows, requestedTool, projectId) {
        var input = 0;
        var output = 0;
        var cacheRead = 0;
        var cacheWrite = 0;
        var events = 0;
        var projects = ({});
        var latest = "";
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i];
            input += row.inputTokens;
            output += row.outputTokens;
            cacheRead += row.cacheReadTokens;
            cacheWrite += row.cacheWriteTokens;
            events += row.eventCount;
            latest = !latest || row.lastSeenAtUtc > latest ? row.lastSeenAtUtc : latest;
            if (!projects[row.projectId]) projects[row.projectId] = {
                projectId: row.projectId,
                alias: aliases[row.projectId] || "",
                sessionCount: 0, eventCount: 0, inputTokens: 0,
                outputTokens: 0, cacheReadTokens: 0, cacheWriteTokens: 0
            };
            projects[row.projectId].sessionCount += 1;
            projects[row.projectId].eventCount += row.eventCount;
            projects[row.projectId].inputTokens += row.inputTokens;
            projects[row.projectId].outputTokens += row.outputTokens;
            projects[row.projectId].cacheReadTokens += row.cacheReadTokens;
            projects[row.projectId].cacheWriteTokens += row.cacheWriteTokens;
        }
        return {
            sessionCount: rows.length,
            eventCount: events,
            inputTokens: input,
            outputTokens: output,
            cacheReadTokens: cacheRead,
            cacheWriteTokens: cacheWrite,
            coverage: rows.some(function(row) { return row.coverage === "partial"; })
                ? "partial" : "reported",
            lastActivityAtUtc: latest,
            modelBreakdown: modelSummary(rows, requestedTool),
            projectBreakdown: projectId ? [] : Object.keys(projects).map(function(key) {
                return projects[key];
            }),
            projectOptions: Object.keys(projects),
            toolId: requestedTool,
            toolEnabled: true,
            importStatus: "complete"
        };
    }

    function dailyFor(rows) {
        var days = ({});
        for (var i = 0; i < rows.length; ++i) {
            var date = new Date(rows[i].lastSeenAtUtc);
            var key = date.getFullYear() + "-" + String(date.getMonth() + 1).padStart(2, "0")
                + "-" + String(date.getDate()).padStart(2, "0");
            if (!days[key]) days[key] = {
                localDate: key, sessionCount: 0, eventCount: 0,
                inputTokens: 0, outputTokens: 0,
                cacheReadTokens: 0, cacheWriteTokens: 0
            };
            days[key].sessionCount += 1;
            days[key].eventCount += rows[i].eventCount;
            days[key].inputTokens += rows[i].inputTokens;
            days[key].outputTokens += rows[i].outputTokens;
            days[key].cacheReadTokens += rows[i].cacheReadTokens;
            days[key].cacheWriteTokens += rows[i].cacheWriteTokens;
        }
        return Object.keys(days).sort().reverse().map(function(key) { return days[key]; });
    }

    function requestImport() { }
    function setToolEnabled(requestedTool, enabled) {
        if (requestedTool === "codex-cli") codexEnabled = !!enabled;
        else if (requestedTool === "claude-code") claudeCodeEnabled = !!enabled;
    }
    function requestProjects(requestId) {
        var rows = [];
        if (toolId === "codex-cli" || toolId === "claude-code") {
            var source = fixtureRows(toolId);
            var seen = ({});
            for (var i = 0; i < source.length; ++i) {
                if (seen[source[i].projectId]) continue;
                seen[source[i].projectId] = true;
                rows.push({
                    toolId: toolId,
                    projectId: source[i].projectId,
                    alias: aliases[source[i].projectId] || "",
                    sessionCount: 2
                });
            }
        }
        Qt.callLater(function() { mediaModel.projectsReady(requestId, rows); });
    }
    function setProjectAlias(projectId, alias) {
        var next = Object.assign({}, aliases);
        next[projectId] = String(alias || "");
        aliases = next;
        return true;
    }
    function projectAlias(projectId) { return aliases[projectId] || ""; }
    function requestSessions(requestId, requestedTool, fromUtc, toUtc,
                             projectId, offset, limit) {
        var rows = filteredRows(requestedTool, fromUtc, toUtc, projectId);
        rows.sort(function(a, b) { return b.lastSeenAtUtc.localeCompare(a.lastSeenAtUtc); });
        var start = Math.max(0, Number(offset) || 0);
        var count = Math.max(1, Number(limit) || 100);
        Qt.callLater(function() {
            mediaModel.sessionsReady(requestId, rows.slice(start, start + count));
        });
    }
    function requestSummary(requestId, requestedTool, fromUtc, toUtc, projectId) {
        var result = summaryFor(
            filteredRows(requestedTool, fromUtc, toUtc, projectId), requestedTool, projectId);
        Qt.callLater(function() { mediaModel.summaryReady(requestId, result); });
    }
    function requestDaily(requestId, requestedTool, fromUtc, toUtc, projectId) {
        var result = dailyFor(filteredRows(requestedTool, fromUtc, toUtc, projectId));
        Qt.callLater(function() { mediaModel.dailyReady(requestId, result); });
    }
    function requestExport(requestId, requestedTool, fromUtc, toUtc,
                           projectId, format, destinationPath) {
        Qt.callLater(function() {
            mediaModel.exportReady(requestId, "demo_export_not_written");
        });
    }
    function clearSessions(requestId) {
        Qt.callLater(function() {
            mediaModel.clearFinished(requestId, true, "");
        });
    }
}
