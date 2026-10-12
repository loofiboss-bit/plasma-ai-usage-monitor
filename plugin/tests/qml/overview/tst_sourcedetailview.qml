import QtQuick
import QtTest
import "../../../../package/contents/ui/views" as Views

TestCase {
    id: tests
    name: "SourceDetailPresentation"
    visible: true
    when: windowShown
    width: 1200
    height: 800

    Component {
        id: detailComponent
        Views.SourceDetailView { width: 1200; height: 800 }
    }

    function test_diagnosticsStartClosedAndResetForAnotherSource() {
        var detail = createTemporaryObject(detailComponent, tests);
        verify(detail !== null);
        verify(!detail.technicalDetailsExpanded);
        verify(!detail.analysisExpanded);
        verify(!detail.dataQualityExpanded);
        detail.technicalDetailsExpanded = true;
        detail.analysisExpanded = true;
        detail.dataQualityExpanded = true;
        detail.sourceId = "codex-cli";
        verify(!detail.technicalDetailsExpanded);
        verify(!detail.analysisExpanded);
        verify(!detail.dataQualityExpanded);
    }

    function test_narrowDetailUsesOnlyVerticalScrolling() {
        var detail = createTemporaryObject(detailComponent, tests, {width: 420, height: 620});
        wait(0);
        compare(detail.contentWidth, detail.availableWidth);
        verify(detail.contentWidth <= 420);
    }

    function test_quotaWindowNamesAreReadable() {
        var detail = createTemporaryObject(detailComponent, tests);
        compare(detail.quotaLabel({window: "five_hour"}), "5-hour limit");
        compare(detail.quotaLabel({window: "weekly"}), "Weekly limit");
        compare(detail.quotaLabel({label: "Custom quota", window: "weekly"}), "Custom quota");
    }

    function test_quotaRemainingDoesNotConvertMissingOrInvalidValuesToZero() {
        var detail = createTemporaryObject(detailComponent, tests);
        compare(detail.quotaRemaining({percentRemaining: 94}), 94);
        compare(detail.quotaRemaining({percentRemaining: 0}), 0);
        compare(detail.quotaRemaining({percentRemaining: 40, available: false}), null);
        compare(detail.quotaRemaining({percentRemaining: null}), null);
        compare(detail.quotaRemaining({}), null);
        compare(detail.quotaRemaining({percentRemaining: ""}), null);
        compare(detail.quotaRemaining({percentRemaining: 101}), null);
        compare(detail.quotaRemaining({percentRemaining: -1}), null);
        compare(detail.quotaRemaining({percentRemaining: NaN}), null);
    }
}
