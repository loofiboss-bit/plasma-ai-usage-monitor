import QtQuick
import QtTest
import org.kde.kirigami as Kirigami
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/Utils.js" as Utils
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "OverviewComponents"

    Component {
        id: costCardComponent
        Monitor.CostSummaryCard { width: 800; summary: ({}) }
    }

    Component {
        id: dailyOverviewStateComponent
        Components.DailyOverviewState {
            dailyState: ({ summary: ({}) })
        }
    }

    Component {
        id: dailySourceCardComponent
        Monitor.DailySourceCard { width: 420; row: ({}) }
    }

    function test_sourceHeadlineKeepsMissingSeparateFromZeroAndCountsIntegral() {
        var card = createTemporaryObject(dailySourceCardComponent, testCase, {
            row: {displayName: "Codex", stableId: "codex-cli", sourceKind: "local_tool",
                  primaryMetricAvailable: true, primaryMetricValue: null, primaryMetricUnit: "token"}
        });
        verify(card !== null);
        compare(card.metricText(), "Unavailable");
        compare(card.exactMetricText(), "Unavailable");
        card.row = Object.assign({}, card.row, {primaryMetricValue: ""});
        compare(card.metricText(), "Unavailable");
        compare(card.exactMetricText(), "Unavailable");
        card.row = Object.assign({}, card.row, {primaryMetricValue: 0});
        compare(card.metricText(), "0 tokens");
        card.row = Object.assign({}, card.row, {primaryMetricValue: 45, primaryMetricUnit: "count"});
        compare(card.metricText(), "45 events");
        card.row = Object.assign({}, card.row, {primaryMetricValue: 1200000000, primaryMetricUnit: "token"});
        verify(card.metricText().indexOf("B") > 0);
        verify(card.exactMetricText().indexOf((1200000000).toLocaleString(Qt.locale(), "f", 0)) >= 0);
    }

    function test_sourceCardMovesMetricsBelowDetailsAtNarrowWidths() {
        var card = createTemporaryObject(dailySourceCardComponent, testCase, {
            row: {displayName: "A long source name for narrow layouts",
                stableId: "codex-cli", sourceKind: "local_tool",
                primaryMetricAvailable: true, primaryMetricValue: 1250,
                primaryMetricUnit: "request"}
        });
        verify(card !== null);
        var metrics = findChild(card, "sourceCardMetricsLayout");
        verify(metrics !== null);
        var widths = [320, 375, 414, 420, 768, 1000];
        for (var i = 0; i < widths.length; ++i) {
            card.width = widths[i];
            var shouldStack = widths[i] < Kirigami.Units.gridUnit * 24;
            compare(metrics.columns, shouldStack ? 1 : 2,
                    "source card width " + widths[i]);
        }
    }

    QtObject {
        id: fakeAntigravity
        property bool installed: true
        property bool limitReached: false
        property string planTier: "pro"
        property string detectedPlanLabel: "Google AI Pro"
        property string syncSourceLabel: "Antigravity local"
        property string connectionState: "stale"
        property bool syncEnabled: true
        property bool syncing: false
        property string syncStatus: "Stale — timeout"
        property var quotaWindows: [
            { label: "Gemini High", modelFamily: "google", percentUsed: 20, percentRemaining: 80, badge: "Live" },
            { label: "Claude Sonnet", modelFamily: "anthropic", availability: "disabled",
              availabilityReason: "Temporarily unavailable", precision: "availability_only", badge: "Unavailable" },
            { label: "GPT-OSS", modelFamily: "openai", precision: "availability_only", badge: "Available" }
        ]
        function availablePlans() { return ["Google AI Pro"]; }
        function planIdForLabel(label) { return label === "Google AI Pro" ? "pro" : "unknown"; }
    }

    Component {
        id: subscriptionCardComponent
        Monitor.SubscriptionToolCard {
            width: 800
            modelData: ({ stableId: "google-antigravity" })
            toolName: "Google Antigravity"
            toolIcon: "applications-science"
            toolColor: "#4285f4"
            monitor: fakeAntigravity
        }
    }

    Component {
        id: quotaRowComponent
        Monitor.QuotaRow { width: 800; rowData: ({}) }
    }

    function test_explicitUnavailableCostDoesNotBecomeZeroSpend() {
        var backend = {
            costSource: "billing_api",
            cost: 0,
            metrics: [{ kind: "cost", available: false, value: 0 }]
        };
        verify(!Utils.hasCompatibleCostData(backend));
        backend.metrics[0].available = true;
        verify(Utils.hasCompatibleCostData(backend));
    }

    function test_liteLlmBadgesArePreciseAndUnique() {
        var badges = Utils.providerOutcomeBadgeKeys(
            "actual_api", "usage_api", "gateway_aggregate", "actual");
        compare(badges.length, 2);
        compare(badges[0], "gateway_usage");
        compare(badges[1], "gateway_spend");
    }

    function test_mixedCurrenciesRemainSeparate() {
        var totals = {};
        Utils.addCurrencyTotal(totals, "USD", 10);
        Utils.addCurrencyTotal(totals, "EUR", 8);
        compare(Object.keys(totals).length, 2);
        var formatted = Utils.formatCurrencyTotals(totals);
        verify(formatted.indexOf("+") >= 0);
    }

    function test_overviewSearchAndAttentionFilterOnlyChangeVisibleRows() {
        var allRows = [
            { stableId: "openai", displayName: "OpenAI", qualityClass: "actual",
                attentionSeverity: "none", nextActionText: "" },
            { stableId: "anthropic", displayName: "Anthropic", qualityClass: "actual",
                attentionSeverity: "warning", nextActionText: "Check permissions" },
            { stableId: "cursor", displayName: "Cursor", qualityClass: "estimated",
                attentionSeverity: "none", nextActionText: "" }
        ];
        var searchMatches = Utils.filterOverviewSourceRows(allRows, "permissions", false);
        compare(searchMatches.length, 1);
        compare(searchMatches[0].stableId, "anthropic");

        var attentionMatches = Utils.filterOverviewSourceRows(allRows, "", true);
        compare(attentionMatches.length, 1);
        compare(attentionMatches[0].stableId, "anthropic");
        compare(allRows.length, 3);
        compare(allRows[0].stableId, "openai");
    }

    function test_overviewGroupsKeepPriorityAndHideEmptyGroups() {
        var rows = [
            {stableId: "codex-cli", sourceKind: "local_tool"},
            {stableId: "anthropic", sourceKind: "provider"},
            {stableId: "claude-code", sourceKind: "local_tool"}
        ];
        var groups = Utils.groupOverviewSourceRows(rows);
        compare(groups.length, 2);
        compare(groups[0].label, "AI tools");
        compare(groups[0].rows.length, 2);
        compare(groups[0].rows[0].stableId, "codex-cli");
        compare(groups[0].rows[1].stableId, "claude-code");
        compare(groups[1].label, "API services");
        compare(groups[1].rows[0].stableId, "anthropic");
        compare(Utils.groupOverviewSourceRows([rows[0]])[0].key, "local_tool");
    }

    function test_dailyFocusUsesQuotaResetFromThatQuotaAndActualSpendToday() {
        var state = createTemporaryObject(dailyOverviewStateComponent, testCase);
        verify(state);
        state.dailyState = ({
            summary: {
                enabledSourceCount: 2,
                reportingUsefulSourceCount: 2,
                lowestActualRemainingQuota: {
                    stableId: "codex-cli", displayName: "Codex",
                    window: "five-hour", percentRemaining: 20,
                    resetAt: new Date(Date.now() + 3600000)
                },
                nearestActualReset: {
                    stableId: "claude-code", displayName: "Claude",
                    window: "weekly", resetAt: new Date(Date.now() + 7200000)
                },
                spendGroups: [
                    {quality: "actual", window: "day", value: 4.5,
                        currency: "USD", sourceNames: ["OpenAI"]},
                    {quality: "actual", window: "month", value: 20,
                        currency: "USD", sourceNames: ["OpenAI"]},
                    {quality: "estimated", window: "day", value: 1,
                        currency: "USD", sourceNames: ["Cursor"]}
                ],
                fixedSubscriptionFees: {USD: 25}
            }
        });
        var facts = state.focusFacts();
        compare(facts.length, 2);
        verify(facts[0].label.indexOf("Codex") >= 0);
        verify(facts[0].label.indexOf("five-hour") >= 0);
        verify(facts[0].value.indexOf("resets in") >= 0);
        verify(facts[1].label.indexOf("today") >= 0);
        verify(facts[1].value.indexOf("4.5") >= 0);
        verify(facts[1].value.indexOf("20.00") < 0);
        verify(facts[1].value.indexOf("1.00") < 0);
    }

    function test_compactNumbersUseLocaleAndNonBreakingSpacing() {
        var formatted = Utils.formatNumber(1250, Qt.locale("sv_SE"));
        verify(formatted.indexOf("\u202fK") > 0);
        verify(formatted.indexOf(",") > 0);
    }

    function test_costCardKeepsTypedCategoriesSeparate() {
        var card = createTemporaryObject(costCardComponent, testCase, {
            summary: {
                spendGroups: [
                    { currency: "USD", value: 0, quality: "actual", window: "month",
                        bounded: false, sourceNames: ["OpenAI"] },
                    { currency: "EUR", value: 2.5, quality: "estimated", window: "day",
                        bounded: false, sourceNames: ["Cursor"] }
                ],
                fixedSubscriptionFees: { USD: 20 }
            }
        });
        verify(card);
        compare(card.spendRows.length, 3);
        compare(card.spendRows[0].group.value, 0);
        compare(card.spendRows[1].group.value, 2.5);
        compare(card.spendRows[2].totals.USD, 20);
        verify(card.spendRows[0].label.indexOf("this month") >= 0);
        verify(card.spendRows[1].label.indexOf("Estimated") >= 0);
    }

    function test_antigravityCardUsesLocalSourceAndDetectedPlan() {
        var card = createTemporaryObject(subscriptionCardComponent, testCase);
        verify(card);
        compare(card.syncSourceText, "Antigravity local");
        compare(card.displayPlanTier(), "Google AI Pro");
        verify(card.stale);
    }

    function test_antigravityAvailabilityRowsDoNotInventProgress() {
        var unavailable = createTemporaryObject(quotaRowComponent, testCase, {
            rowData: fakeAntigravity.quotaWindows[1]
        });
        verify(unavailable);
        verify(!unavailable.hasProgress());
        compare(unavailable.valueText(), "Unavailable");

        var available = createTemporaryObject(quotaRowComponent, testCase, {
            rowData: fakeAntigravity.quotaWindows[2]
        });
        verify(available);
        verify(!available.hasProgress());
        compare(available.valueText(), "Available");
    }
}
