import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/Utils.js" as Utils

TestCase {
    id: testCase
    name: "OverviewComponents"

    Component {
        id: costCardComponent
        Monitor.CostSummaryCard { width: 800; summary: ({}) }
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

    function test_compactNumbersUseLocaleAndNonBreakingSpacing() {
        var formatted = Utils.formatNumber(1250, Qt.locale("sv_SE"));
        verify(formatted.indexOf("\u202fK") > 0);
        verify(formatted.indexOf(",") > 0);
    }

    function test_costCardKeepsTypedCategoriesSeparate() {
        var card = createTemporaryObject(costCardComponent, testCase, {
            summary: {
                actualSpendTotals: { USD: 0 },
                estimatedSpendTotals: { EUR: 2.5 },
                fixedSubscriptionFees: { USD: 20 }
            }
        });
        verify(card);
        compare(card.spendRows.length, 3);
        compare(card.spendRows[0].totals.USD, 0);
        compare(card.spendRows[1].totals.EUR, 2.5);
        compare(card.spendRows[2].totals.USD, 20);
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
