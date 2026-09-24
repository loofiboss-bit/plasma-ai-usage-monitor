import QtQuick
import QtTest
import "../../../../package/contents/ui/SourceHealth.js" as SourceHealth

TestCase {
    name: "SourceHealth"

    function test_retryAfterDisablesActionUntilPermittedTime() {
        var now = Date.parse("2026-09-24T12:00:00Z");
        var retryAt = "2026-09-24T12:30:00Z";

        verify(SourceHealth.retryBlocked(retryAt, now));
        verify(!SourceHealth.canRequestAction(retryAt, now));
        verify(!SourceHealth.retryBlocked(retryAt, Date.parse("2026-09-24T12:30:01Z")));
        verify(SourceHealth.canRequestAction(retryAt, Date.parse("2026-09-24T12:30:01Z")));
    }

    function test_missingOrInvalidRetryAfterDoesNotBlockAction() {
        verify(!SourceHealth.retryBlocked(null, Date.now()));
        verify(SourceHealth.canRequestAction(undefined, Date.now()));
        verify(!SourceHealth.retryBlocked("not-a-date", Date.now()));
    }
}
