import QtQuick
import QtTest
import "../../../../package/contents/ui" as Monitor

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

    Component {
        id: heatmapComponent
        Monitor.ActivityHeatmap { width: 760; height: 110 }
    }

    Component {
        id: ratioCardComponent
        Monitor.OutputInputRatioCard { width: 320 }
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
        var card = createTemporaryObject(ratioCardComponent, testCase, { outputInputRatio: 0.25 });
        var highCard = createTemporaryObject(ratioCardComponent, testCase, { outputInputRatio: 2.5 });
        verify(card);
        verify(highCard);
        compare(findChild(card, "ratioTitle").text, "Output / Input Ratio");
        compare(findChild(card, "ratioDescription").text, "Output tokens divided by input tokens");
        compare(findChild(card, "ratioValue").color.toString(),
                findChild(highCard, "ratioValue").color.toString());
    }
}
