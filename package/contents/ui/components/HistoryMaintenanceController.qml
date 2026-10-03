import QtQuick

Item {
    id: controller
    visible: false
    required property var database
    property var storage: ({})
    property string storageRequest: ""
    property string pruneRequest: ""
    property string exportRequest: ""
    property var lastResult: ({})
    readonly property bool busy: storageRequest.length > 0 || pruneRequest.length > 0 || exportRequest.length > 0
    readonly property bool storageAvailable: storage.ok === true
    property Connections databaseConnection: Connections {
        target: controller.database
        function onMaintenanceFinished(requestId, result) {
            if (requestId === controller.pruneRequest) {
                controller.pruneRequest = "";
                controller.lastResult = result;
                if (result.ok) controller.storage = result;
                else controller.refreshStorage();
            } else if (requestId === controller.storageRequest) {
                controller.storageRequest = "";
                controller.storage = result;
            }
        }
        function onExportFinished(requestId, result) {
            if (requestId !== controller.exportRequest) return;
            controller.exportRequest = "";
            controller.lastResult = result;
            controller.refreshStorage();
        }
    }
    function requestId(prefix) { return prefix + Date.now() + "-" + Math.random(); }
    function refreshStorage() {
        if (storageRequest.length > 0 || busy) return;
        storageRequest = requestId("storage-");
        database.requestStorageStatus(storageRequest);
    }
    function prune() {
        if (busy) return;
        storageRequest = ""; // Ignore storage results captured before this maintenance operation.
        pruneRequest = requestId("manual-prune-");
        database.requestPrune(pruneRequest);
    }
    function exportFiles(directory, formats) {
        if (busy) return;
        storageRequest = "";
        exportRequest = requestId("manual-");
        database.requestExportAll(exportRequest, directory, formats);
    }
    Component.onCompleted: refreshStorage()
}
