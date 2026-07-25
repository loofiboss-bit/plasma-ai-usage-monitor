import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "DailyControl"

    function i18n(message) {
        var result = message;
        for (var i = 1; i < arguments.length; i++)
            result = result.replace("%" + i, arguments[i]);
        return result;
    }

    function i18np(singular, plural, count) {
        var args = [count === 1 ? singular : plural];
        for (var i = 2; i < arguments.length; i++) args.push(arguments[i]);
        if (args.length === 1) args.push(count);
        return i18n.apply(null, args);
    }

    QtObject {
        id: fakeDailyState
        property var rows: ({})
        property var ids: []
        property var summary: ({})
        signal sourceChanged(string stableId)
        signal countChanged()

        function prioritizedSourceIds() { return ids; }
        function source(stableId) { return rows[stableId] || ({}); }
    }

    Components.DailyOverviewState {
        id: dailyPresentation
        dailyState: fakeDailyState
    }

    Component {
        id: statusHeaderComponent
        Components.StatusHeader { width: 800; presentation: dailyPresentation }
    }

    Component {
        id: attentionComponent
        Components.AttentionList { width: 800; presentation: dailyPresentation }
    }

    Component {
        id: quotaComponent
        Monitor.QuotaResetCard { width: 800; presentation: dailyPresentation }
    }

    Component {
        id: spendComponent
        Monitor.CostSummaryCard { width: 800; summary: dailyPresentation.summary }
    }

    Component {
        id: sourceComponent
        Monitor.DailySourceCard { width: 800; row: ({}) }
    }

    function row(id, quality, options) {
        var values = options || {};
        return {
            stableId: id,
            displayName: values.displayName || id,
            sourceKind: values.sourceKind || "provider",
            qualityClass: quality,
            freshnessState: values.freshnessState || "fresh",
            attentionSeverity: values.attentionSeverity || "none",
            attentionReasonKey: values.attentionReasonKey || "none",
            nextActionKey: values.nextActionKey || "none",
            primaryMetricAvailable: values.primaryMetricAvailable === true,
            primaryMetricValue: values.primaryMetricValue,
            primaryMetricUnit: values.primaryMetricUnit || ""
        };
    }

    function summary(options) {
        var values = options || {};
        return {
            enabledSourceCount: values.enabledSourceCount || 0,
            reportingUsefulSourceCount: values.reportingUsefulSourceCount || 0,
            actualSourceCount: values.actualSourceCount || 0,
            estimatedSourceCount: values.estimatedSourceCount || 0,
            balanceSourceCount: values.balanceSourceCount || 0,
            connectivityOnlySourceCount: values.connectivityOnlySourceCount || 0,
            attentionSourceCount: values.attentionSourceCount || 0,
            staleSourceCount: values.staleSourceCount || 0,
            highestSeverity: values.highestSeverity || "none",
            mostUrgentSource: values.mostUrgentSource || ({}),
            lowestRemainingQuota: values.lowestRemainingQuota || ({}),
            nearestReset: values.nearestReset || ({}),
            lowestActualRemainingQuota: values.lowestActualRemainingQuota || ({}),
            nearestActualReset: values.nearestActualReset || ({}),
            actualSpendTotals: values.actualSpendTotals || ({}),
            estimatedSpendTotals: values.estimatedSpendTotals || ({}),
            fixedSubscriptionFees: values.fixedSubscriptionFees || ({}),
            providerActualSpendTotals: values.providerActualSpendTotals || ({}),
            providerDailyActualSpendTotals: values.providerDailyActualSpendTotals || ({}),
            remainingRequests: values.remainingRequests || ({})
        };
    }

    function load(rows, activeSummary) {
        fakeDailyState.rows = rows;
        fakeDailyState.ids = Object.keys(rows);
        fakeDailyState.summary = activeSummary;
        dailyPresentation.revision++;
    }

    function init() { load({}, summary({})); }

    function test_legacyPanelModesMapToCanonicalModes() {
        compare(dailyPresentation.normalizeCompactMode("count"), "active-sources");
        compare(dailyPresentation.normalizeCompactMode("critical"), "attention");
        compare(dailyPresentation.normalizeCompactMode("cost"), "actual-spend");
        compare(dailyPresentation.normalizeCompactMode("dailycost"), "dailycost");
        compare(dailyPresentation.normalizeCompactMode("requests"), "requests");
        compare(dailyPresentation.normalizeCompactMode("unknown"), "icon");
    }

    function test_panelModesPreserveUnavailableAndAvailableZero() {
        load({}, summary({ enabledSourceCount: 1 }));
        compare(dailyPresentation.compactText("lowest-quota"), "\u2014");
        compare(dailyPresentation.compactText("next-reset"), "\u2014");
        compare(dailyPresentation.compactText("actual-spend"), "\u2014");
        compare(dailyPresentation.compactText("dailycost"), "\u2014");
        compare(dailyPresentation.compactText("requests"), "\u2014");
        compare(dailyPresentation.compactStatusKey(), "unverified");

        load({}, summary({
            enabledSourceCount: 1,
            reportingUsefulSourceCount: 1,
            providerActualSpendTotals: { USD: 0 },
            providerDailyActualSpendTotals: { USD: 0 },
            remainingRequests: { stableId: "openai", value: 0 },
            lowestActualRemainingQuota: {
                stableId: "openai", displayName: "OpenAI", percentRemaining: 0
            }
        }));
        verify(dailyPresentation.compactText("actual-spend") !== "\u2014");
        verify(dailyPresentation.compactText("dailycost") !== "\u2014");
        compare(dailyPresentation.compactText("requests"), "0 req");
        compare(dailyPresentation.compactText("lowest-quota"), "0% · OpenAI");
        compare(dailyPresentation.compactText("active-sources"), "1");
        compare(dailyPresentation.compactStatusKey(), "healthy");

        load({}, summary({ enabledSourceCount: 1, connectivityOnlySourceCount: 1 }));
        compare(dailyPresentation.compactText("attention"), "Connectivity only");
        compare(dailyPresentation.compactStatusKey(), "connectivity");
    }

    function test_rowsAreGroupedFromDailyStateOnly() {
        var critical = row("openai", "actual", {
            attentionSeverity: "critical", attentionReasonKey: "quota_critical",
            nextActionKey: "review_quota"
        });
        var local = row("codex-cli", "estimated", { sourceKind: "local_tool" });
        var connection = row("anthropic", "connectivity_only");
        load({ openai: critical, "codex-cli": local, anthropic: connection }, summary({
            enabledSourceCount: 3, reportingUsefulSourceCount: 2,
            connectivityOnlySourceCount: 1, attentionSourceCount: 1,
            highestSeverity: "critical", mostUrgentSource: critical
        }));

        compare(dailyPresentation.actualRows.length, 1);
        compare(dailyPresentation.estimatedRows.length, 1);
        compare(dailyPresentation.connectivityRows.length, 1);
        compare(dailyPresentation.topAction.stableId, "openai");
        compare(dailyPresentation.headline(), "Your daily monitor needs attention");
    }

    function test_headlineUsesOnlyRelevantFacts() {
        var resetAt = new Date(Date.now() + 60 * 60 * 1000);
        load({}, summary({
            enabledSourceCount: 2, reportingUsefulSourceCount: 2,
            lowestActualRemainingQuota: {
                stableId: "openai", displayName: "OpenAI", percentRemaining: 12
            },
            nearestActualReset: {
                stableId: "codex-cli", displayName: "Codex CLI", resetAt: resetAt
            }
        }));
        var facts = dailyPresentation.factRows();
        compare(facts.length, 2);
        compare(facts[0].value, "12%");
        verify(facts[1].label.indexOf("Codex CLI") >= 0);

        var header = createTemporaryObject(statusHeaderComponent, testCase);
        verify(header);
        compare(findChild(header, "dailyHeadline").text, "2 sources are reporting");
    }

    function test_onlyTopActionGetsRecoveryButton() {
        var first = row("openai", "unavailable", {
            attentionSeverity: "critical", attentionReasonKey: "authentication",
            nextActionKey: "replace_credentials"
        });
        var second = row("codex-cli", "unavailable", {
            sourceKind: "local_tool", attentionSeverity: "warning",
            attentionReasonKey: "stale_data", nextActionKey: "refresh_stale_data"
        });
        load({ openai: first, "codex-cli": second }, summary({
            enabledSourceCount: 2, attentionSourceCount: 2,
            highestSeverity: "critical", mostUrgentSource: first
        }));
        var attention = createTemporaryObject(attentionComponent, testCase);
        verify(attention);
        compare(attention.row.stableId, "openai");
        compare(findChild(attention, "topActionTitle").text, "openai");
        compare(findChild(attention, "topActionButton").text, "Add credential");
    }

    function test_publishedLimitDoesNotRenderAsLiveQuota() {
        load({}, summary({
            enabledSourceCount: 1,
            lowestRemainingQuota: {
                stableId: "published", displayName: "Published", percentRemaining: 10
            }
        }));
        var card = createTemporaryObject(quotaComponent, testCase);
        verify(card);
        verify(!card.visible);

        fakeDailyState.summary = summary({
            enabledSourceCount: 1,
            lowestActualRemainingQuota: {
                stableId: "live", displayName: "Live", percentRemaining: 0
            }
        });
        dailyPresentation.revision++;
        tryVerify(function() { return card.quota.stableId === "live"; });
        compare(findChild(card, "lowestQuotaValue").text, "0% remaining");
    }

    function test_spendCategoriesStaySeparateAndKeepAvailableZero() {
        load({}, summary({
            enabledSourceCount: 1,
            actualSpendTotals: { USD: 0 },
            estimatedSpendTotals: { EUR: 2.5 },
            fixedSubscriptionFees: { USD: 20 }
        }));
        var card = createTemporaryObject(spendComponent, testCase);
        verify(card);
        compare(card.spendRows.length, 3);
        compare(card.spendRows[0].totals.USD, 0);
        compare(card.spendRows[1].totals.EUR, 2.5);
        compare(card.spendRows[2].totals.USD, 20);
    }

    function test_toolOnlyCardUsesDailyStateMetric() {
        var source = createTemporaryObject(sourceComponent, testCase, {
            row: row("codex-cli", "estimated", {
                sourceKind: "local_tool", primaryMetricAvailable: true,
                primaryMetricValue: 75, primaryMetricUnit: "percent_remaining"
            })
        });
        verify(source);
        compare(source.stateText(), "Local estimate");
        compare(source.metricText(), "75% left");
        compare(findChild(source, "sourceMetricValue").text, "75% left");
        verify(source.logoSource().toString().indexOf("tools/codex-cli.svg") >= 0);
    }

    function test_providerCardsUseTheirOwnLogos() {
        var source = createTemporaryObject(sourceComponent, testCase, {
            row: row("openrouter", "actual", {
                displayName: "OpenRouter",
                primaryMetricAvailable: true,
                primaryMetricValue: 0
            })
        });
        verify(source);
        verify(source.logoSource().toString().indexOf("providers/openrouter.svg") >= 0);
        verify(findChild(source, "sourceLogo"));
    }
}
