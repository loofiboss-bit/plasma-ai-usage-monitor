import QtQuick

QtObject {
    id: guard
    property var window: null
    property bool failedApplyInProgress: false
    signal retryReady()

    // Plasma ignores saveConfig's return and OK immediately calls close().
    // Reject that close only; a later deliberate Cancel remains available.
    function protectFailedApply() {
        failedApplyInProgress = true;
        Qt.callLater(function() {
            guard.failedApplyInProgress = false;
            guard.retryReady();
        });
    }

    property Connections closeConnection: Connections {
        target: guard.window
        function onClosing(event) {
            if (guard.failedApplyInProgress) event.accepted = false;
        }
    }
}
