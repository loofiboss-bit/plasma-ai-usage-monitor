import QtQuick
import QtTest
import org.kde.plasma.extras as PlasmaExtras
import "../../../../package/contents/ui/components" as Components
import "../../../../package/contents/ui" as Monitor
import "../../../../package/contents/ui/Utils.js" as Utils
import "fixtures/DailyTruthFixtures.js" as Fixtures

TestCase {
    id: testCase
    name: "DailyTruth"

    function i18n(message) {
        var result = message;
        for (var i = 1; i < arguments.length; i++)
            result = result.replace("%" + i, arguments[i]);
        return result;
    }

    function i18np(singular, plural, count) {
        return i18n(count === 1 ? singular : plural, count);
    }

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
        id: heatmapComponent
        Monitor.ActivityHeatmap { width: 760; height: 110 }
    }

    Component {
        id: ratioCardComponent
        Monitor.EfficiencyMetricCard { width: 320 }
    }

    function loadFixture(fixture) {
        fakeReadiness.rows = fixture.sources;
        overviewState.providers = fixture.providers;
        overviewState.tools = fixture.tools;
        overviewState.revision++;
    }

    function cleanup() {
        loadFixture(Fixtures.empty());
    }

    function test_sourceMatrix_data() {
        return [
            { tag: "provider-only", fixture: Fixtures.providerOnly(), enabled: 1, useful: 1, connectivity: 0, attention: 0 },
            { tag: "tool-only", fixture: Fixtures.toolOnly(), enabled: 1, useful: 1, connectivity: 0, attention: 0 },
            { tag: "mixed", fixture: Fixtures.mixed(), enabled: 2, useful: 2, connectivity: 0, attention: 0 },
            { tag: "connectivity-only", fixture: Fixtures.connectivityOnly(), enabled: 1, useful: 0, connectivity: 1, attention: 0 },
            { tag: "needs-configuration", fixture: Fixtures.needsConfiguration(), enabled: 1, useful: 0, connectivity: 0, attention: 1 },
            { tag: "no-sources", fixture: Fixtures.empty(), enabled: 0, useful: 0, connectivity: 0, attention: 0 }
        ];
    }

    function test_sourceMatrix(data) {
        loadFixture(data.fixture);
        compare(overviewState.summary.enabled, data.enabled);
        compare(overviewState.summary.useful, data.useful);
        compare(overviewState.summary.connectivity, data.connectivity);
        compare(overviewState.summary.attention, data.attention);
        compare(overviewState.summary.verified, data.useful + data.connectivity);
    }

    function test_summaryReportsUsefulConnectivityAndAttention() {
        var fixture = Fixtures.mixed();
        var pending = Fixtures.needsConfiguration();
        fixture.providers.push(pending.providers[0]);
        fixture.sources.openai_pending = pending.sources.openai;
        fixture.providers[1].configKey = "openai_pending";
        fixture.providers[1].name = "openai_pending";
        loadFixture(fixture);
        var text = overviewState.summaryText();
        verify(text.indexOf("2 of 3 active sources") >= 0);
        verify(text.indexOf("1 needs attention") >= 0);
    }

    function test_actualCostAvailabilityAndCurrencies() {
        var providers = [
            Fixtures.provider("zero", [Fixtures.metric("cost", 0, true,
                { window: "current", source: "billing_api", currency: "USD" })]),
            Fixtures.provider("eur", [Fixtures.metric("cost", 3, true,
                { window: "current", source: "usage_api", currency: "EUR" })]),
            Fixtures.provider("daily", [Fixtures.metric("cost", 2, true,
                { window: "day", source: "billing_api", currency: "USD" })]),
            Fixtures.provider("unavailable", [Fixtures.metric("cost", 0, false,
                { window: "current", source: "billing_api", currency: "USD" })]),
            Fixtures.provider("estimated", [Fixtures.metric("cost", 99, true,
                { window: "current", source: "estimated_pricing", currency: "USD" })])
        ];
        providers[4].backend.costSource = "estimated_from_usage";

        var current = Utils.actualCostTotals(providers, "current");
        compare(Object.keys(current).length, 2);
        compare(current.USD, 0);
        compare(current.EUR, 3);

        var daily = Utils.actualCostTotals(providers, "day");
        compare(Object.keys(daily).length, 1);
        compare(daily.USD, 2);
    }

    function test_requestZeroDiffersFromUnavailable() {
        var available = Fixtures.provider("available", [
            Fixtures.metric("request_remaining", 0, true)
        ]);
        var result = Utils.requestsRemaining([available]);
        verify(result.available);
        compare(result.value, 0);

        var unavailable = Fixtures.provider("unavailable", [
            Fixtures.metric("request_remaining", 0, false)
        ]);
        result = Utils.requestsRemaining([unavailable]);
        verify(!result.available);
        compare(result.value, 0);
    }

    function test_badgeStatusUsesVerifiedSourcesAndTypedQuota() {
        loadFixture(Fixtures.providerOnly());
        compare(Utils.compactStatus(overviewState.summary, overviewState.providers, [], 80, 95), "healthy");

        loadFixture(Fixtures.toolOnly());
        compare(Utils.compactStatus(overviewState.summary, [], overviewState.tools, 80, 95), "healthy");

        loadFixture(Fixtures.connectivityOnly());
        compare(Utils.compactStatus(overviewState.summary, overviewState.providers, [], 80, 95), "healthy");

        loadFixture(Fixtures.needsConfiguration());
        compare(Utils.compactStatus(overviewState.summary, overviewState.providers, [], 80, 95), "warning");

        var unverifiedSummary = { enabled: 1, verified: 0, attention: 0 };
        compare(Utils.compactStatus(unverifiedSummary, [], [], 80, 95), "unverified");

        var provider = Fixtures.provider("warning", [
            Fixtures.metric("request_limit", 100, true),
            Fixtures.metric("request_remaining", 15, true)
        ]);
        var verifiedSummary = { enabled: 1, verified: 1, attention: 0 };
        compare(Utils.compactStatus(verifiedSummary, [provider], [], 80, 95), "warning");

        var tool = Fixtures.tool("critical", {
            installed: true,
            usageLimit: 0,
            percentUsed: 0,
            quotaWindows: [{ limit: 100, percentUsed: 98, precision: "exact" }]
        });
        compare(Utils.compactStatus(verifiedSummary, [], [tool], 80, 95), "critical");
    }

    function test_unavailableQuotaDoesNotCreateWarning() {
        var provider = Fixtures.provider("unknown", [
            Fixtures.metric("request_limit", 100, true),
            Fixtures.metric("request_remaining", 0, false)
        ]);
        compare(Utils.providerQuotaPercent(provider.backend), -1);

        var tool = Fixtures.tool("unknown-tool", {
            installed: true,
            usageLimit: 0,
            percentUsed: 99,
            quotaWindows: [{ percentUsed: 99, precision: "availability_only" }]
        });
        compare(Utils.toolQuotaPercent(tool.monitor), -1);
    }

    function test_heatmapKeepsMissingAndExplicitZeroDistinct() {
        var heatmap = createTemporaryObject(heatmapComponent, testCase, {
            activityData: [
                { date: "2026-07-20", value: 0 },
                { date: "2026-07-21", value: 5 }
            ],
            maxIntensity: 5
        });
        verify(heatmap);

        var missing = heatmap.valueForDate("2026-07-19");
        var zero = heatmap.valueForDate("2026-07-20");
        var positive = heatmap.valueForDate("2026-07-21");
        verify(!missing.recorded);
        verify(zero.recorded);
        compare(zero.value, 0);
        verify(positive.recorded);
        compare(heatmap.getIntensityColor(missing.value, missing.recorded).toString(),
                heatmap.getIntensityColor(zero.value, zero.recorded).toString());
        verify(heatmap.getIntensityColor(positive.value, positive.recorded).toString()
               !== heatmap.getIntensityColor(zero.value, zero.recorded).toString());
    }

    function test_ratioCardIsNeutral() {
        var card = createTemporaryObject(ratioCardComponent, testCase, { efficiencyRatio: 0.25 });
        var highCard = createTemporaryObject(ratioCardComponent, testCase, { efficiencyRatio: 2.5 });
        verify(card);
        verify(highCard);
        compare(findChild(card, "ratioTitle").text, "Output / Input Ratio");
        compare(findChild(card, "ratioDescription").text, "Output tokens divided by input tokens");
        compare(findChild(card, "ratioValue").color.toString(),
                findChild(highCard, "ratioValue").color.toString());
    }
}
