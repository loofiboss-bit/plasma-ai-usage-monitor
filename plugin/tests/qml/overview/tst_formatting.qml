import QtQuick
import QtTest
import "../../../../package/contents/ui/Utils.js" as Utils
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "FormattingAndCoreComponents"

    Component {
        id: keyValueRowComp
        Components.KeyValueRow {
            width: 300
            label: "Test Label"
            context: "Test Context"
            value: "USD 12.50"
            valueSubtitle: "today"
        }
    }

    Component {
        id: metricTileComp
        Components.MetricTile {
            width: 150
            title: "Input Tokens"
            value: 45000
            isCount: false
        }
    }

    Component {
        id: collapsibleSectionComp
        Components.CollapsibleSection {
            width: 400
            title: "Analysis"
            summaryText: "3 metrics"
            expanded: false
        }
    }

    function test_formatWhenRelative() {
        var now = new Date(2026, 9, 12, 12, 0, 0).getTime(); // Oct 12, 2026 12:00
        var todayEarlier = new Date(2026, 9, 12, 8, 30, 0).getTime();
        var yesterday = new Date(2026, 9, 11, 15, 0, 0).getTime();
        var tomorrow = new Date(2026, 9, 13, 10, 0, 0).getTime();
        var older = new Date(2026, 9, 5, 9, 15, 0).getTime();

        var resToday = Utils.formatWhen(todayEarlier, "relative", null, now);
        verify(resToday.indexOf("today") >= 0);
        verify(resToday.indexOf("08:30") >= 0);

        var resYesterday = Utils.formatWhen(yesterday, "relative", null, now);
        verify(resYesterday.indexOf("yesterday") >= 0);
        verify(resYesterday.indexOf("15:00") >= 0);

        var resTomorrow = Utils.formatWhen(tomorrow, "relative", null, now);
        verify(resTomorrow.indexOf("tomorrow") >= 0);

        var resOlder = Utils.formatWhen(older, "relative", null, now);
        verify(resOlder.indexOf("5") >= 0);
        verify(resOlder.indexOf("Oct") >= 0 || resOlder.indexOf("okt") >= 0);

        // Invalid timestamp returns empty
        compare(Utils.formatWhen("", "relative"), "");
        compare(Utils.formatWhen(null, "relative"), "");
        compare(Utils.formatWhen(undefined, "relative"), "");
    }

    function test_formatDuration() {
        compare(Utils.formatDuration(30000), "< 1 min");
        compare(Utils.formatDuration(120000), "2 minutes");
        compare(Utils.formatDuration(3600000), "1 hour");
        compare(Utils.formatDuration(5400000), "1 h 30 min");
        compare(Utils.formatDuration(90000000), "1 d 1 h");
    }

    function test_formatResetCountdown() {
        var now = new Date(2026, 9, 12, 10, 0, 0).getTime();
        var resetAt = new Date(2026, 9, 12, 12, 15, 0).getTime();
        var text = Utils.formatResetCountdown(resetAt, now);
        verify(text.indexOf("Resets in") >= 0);
        verify(text.indexOf("2 h 15 min") >= 0);
        verify(text.indexOf("today 12:15") >= 0);
    }

    function test_coreComponentsInstantiate() {
        var row = createTemporaryObject(keyValueRowComp, testCase);
        verify(row !== null);
        compare(row.label, "Test Label");
        compare(row.value, "USD 12.50");

        var tile = createTemporaryObject(metricTileComp, testCase);
        verify(tile !== null);
        compare(tile.title, "Input Tokens");
        verify(tile.displayValue().indexOf("45") >= 0);

        var section = createTemporaryObject(collapsibleSectionComp, testCase);
        verify(section !== null);
        compare(section.title, "Analysis");
        compare(section.expanded, false);
        section.expanded = true;
        compare(section.expanded, true);
    }
}
