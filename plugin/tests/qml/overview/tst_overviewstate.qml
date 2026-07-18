import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/Utils.js" as Utils
import "fixtures/OverviewStateFixtures.js" as Fixtures

TestCase {
    id: testCase
    name: "OverviewState"

    QtObject {
        id: fakeReadiness
        property var rows: ({})
        signal sourceChanged(string stableId)

        function source(stableId) {
            return rows[stableId] || ({});
        }
    }

    Components.OverviewState {
        id: overviewState
        readinessModel: fakeReadiness
    }

    Component {
        id: costCardComponent
        Monitor.CostSummaryCard { width: 800 }
    }

    function loadFixture(fixture) {
        fakeReadiness.rows = fixture.sources;
        overviewState.providers = fixture.providers;
        overviewState.tools = fixture.tools || [];
        overviewState.revision++;
    }

    function cleanup() {
        loadFixture(Fixtures.empty());
    }

    function test_qualityFixtures_data() {
        return [
            { tag: "actual", fixture: Fixtures.actual(), actual: 1, estimate: 0, balance: 0, connectivity: 0, attention: 0 },
            { tag: "estimate", fixture: Fixtures.estimate(), actual: 0, estimate: 1, balance: 0, connectivity: 0, attention: 0 },
            { tag: "balance", fixture: Fixtures.balance(), actual: 0, estimate: 0, balance: 1, connectivity: 0, attention: 0 },
            { tag: "connectivity", fixture: Fixtures.connectivity(), actual: 0, estimate: 0, balance: 0, connectivity: 1, attention: 0 },
            { tag: "stale", fixture: Fixtures.stale(), actual: 0, estimate: 0, balance: 0, connectivity: 0, attention: 1 },
            { tag: "error", fixture: Fixtures.error(), actual: 0, estimate: 0, balance: 0, connectivity: 0, attention: 1 },
            { tag: "empty", fixture: Fixtures.empty(), actual: 0, estimate: 0, balance: 0, connectivity: 0, attention: 0 }
        ];
    }

    function test_qualityFixtures(data) {
        loadFixture(data.fixture);
        compare(overviewState.summary.actual, data.actual);
        compare(overviewState.summary.estimate, data.estimate);
        compare(overviewState.summary.balance, data.balance);
        compare(overviewState.summary.connectivity, data.connectivity);
        compare(overviewState.summary.attention, data.attention);
    }

    function test_connectivityUnknownsAreNotUsefulZeroes() {
        var fixture = Fixtures.connectivity();
        loadFixture(fixture);
        verify(!overviewState.providerHasUsefulMetrics(fixture.providers[0]));
        compare(overviewState.reportingProviders.length, 0);
        compare(overviewState.connectivityProviders.length, 1);
    }

    function test_actualZeroRemainsAnAvailableMetric() {
        var fixture = Fixtures.actual();
        loadFixture(fixture);
        verify(overviewState.providerHasUsefulMetrics(fixture.providers[0]));
        compare(overviewState.summary.actual, 1);
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

    function test_staleUsefulProviderStaysVisibleAndActionable() {
        loadFixture(Fixtures.stale());
        compare(overviewState.reportingProviders.length, 1);
        compare(overviewState.attentionRows.length, 1);
        compare(overviewState.attentionRows[0].nextActionKey, "refresh_stale_data");
    }

    function test_liteLlmBadgesArePreciseAndUnique() {
        var badges = Utils.providerOutcomeBadgeKeys(
            "actual_api", "usage_api", "gateway_aggregate", "actual");
        compare(badges.length, 2);
        compare(badges[0], "gateway_usage");
        compare(badges[1], "gateway_spend");
    }

    function test_mixedCurrenciesRemainSeparate() {
        var fixture = Fixtures.mixedCurrency();
        loadFixture(fixture);
        var totals = {};
        Utils.addCurrencyTotal(totals, "USD", fixture.providers[0].backend.cost);
        Utils.addCurrencyTotal(totals, "EUR", fixture.providers[1].backend.cost);
        compare(Object.keys(totals).length, 2);
        var formatted = Utils.formatCurrencyTotals(totals);
        verify(formatted.indexOf("+") >= 0);
    }

    function test_compactNumbersUseLocaleAndNonBreakingSpacing() {
        var formatted = Utils.formatNumber(1250, Qt.locale("sv_SE"));
        verify(formatted.indexOf("\u202fK") > 0);
        verify(formatted.indexOf(",") > 0);
    }

    function test_costCardFiltersNonPositiveRowsBeforeLayout() {
        var card = createTemporaryObject(costCardComponent, testCase, {
            providers: [
                { name: "Actual", enabled: true, color: "red", backend: {
                    connected: true, costSource: "actual_api", cost: 4.25,
                    monthlyCost: 4.25, dailyCost: 0.5, currency: "USD",
                    metrics: [{ kind: "cost", available: true, value: 4.25 }]
                }},
                { name: "Zero", enabled: true, color: "blue", backend: {
                    connected: true, costSource: "actual_api", cost: 0,
                    monthlyCost: 0, dailyCost: 0, currency: "USD",
                    metrics: [{ kind: "cost", available: true, value: 0 }]
                }},
                { name: "Disabled", enabled: false, backend: { connected: true } }
            ],
            subscriptionTools: [
                { name: "Paid", enabled: true, monitor: {
                    hasSubscriptionCost: true, subscriptionCost: 20
                }},
                { name: "Free", enabled: true, monitor: {
                    hasSubscriptionCost: true, subscriptionCost: 0
                }}
            ]
        });
        verify(card);
        compare(card.providerRows.length, 1);
        compare(card.providerRows[0].provider.name, "Actual");
        compare(card.subscriptionRows.length, 1);
        compare(card.subscriptionRows[0].tool.name, "Paid");

        card.costViewMode = 1;
        compare(card.providerRows.length, 1);
        compare(card.subscriptionRows.length, 0);
    }
}
