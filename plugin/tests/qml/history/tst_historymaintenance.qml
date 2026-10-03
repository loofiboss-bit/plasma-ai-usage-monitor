import QtQuick
import QtTest
import "../../../../package/contents/ui/components" as Components

TestCase {
    name: "HistoryMaintenance"
    QtObject {
        id: database
        property int exports: 0
        property int prunes: 0
        property string exportId: ""
        property string pruneId: ""
        property string storageId: ""
        signal exportFinished(string requestId, var result)
        signal maintenanceFinished(string requestId, var result)
        function requestExportAll(requestId, directory, formats) { exports++; exportId = requestId; }
        function requestPrune(requestId) { prunes++; pruneId = requestId; }
        function requestStorageStatus(requestId) { storageId = requestId; }
    }
    Components.HistoryMaintenanceController { id: controller; database: database }
    function init() {
        controller.exportRequest = "";
        controller.pruneRequest = "";
        controller.storageRequest = "";
        database.exports = 0;
        database.prunes = 0;
    }
    function test_export_busy_and_matching_result() {
        controller.exportFiles("/temporary", ["json"]);
        verify(controller.busy);
        controller.exportFiles("/temporary", ["json"]);
        controller.prune();
        compare(database.exports, 1);
        compare(database.prunes, 0);
        database.exportFinished("old", {status: "success"});
        verify(controller.busy);
        database.exportFinished(database.exportId, {ok: false, status: "partial", paths: ["/temporary/export.json"], errorKey: "export-write-failed"});
        compare(controller.exportRequest, "");
        compare(controller.lastResult.status, "partial");
        verify(controller.storageRequest.length > 0);
        verify(controller.busy);
        database.maintenanceFinished(database.storageId, {ok: true, status: "success", sources: []});
        verify(!controller.busy);
    }
    function test_storage_and_prune_refresh() {
        controller.refreshStorage();
        database.maintenanceFinished(database.storageId, {ok: true, status: "success", databaseBytes: 4096, walBytes: 1024, sources: ["OpenAI"]});
        verify(controller.storageAvailable);
        compare(controller.storage.walBytes, 1024);
        controller.prune();
        verify(controller.busy);
        database.maintenanceFinished(database.pruneId, {ok: true, status: "success", rowsDeleted: 5, databaseBytes: 4096, walBytes: 1024, sources: []});
        verify(!controller.busy);
        compare(controller.lastResult.rowsDeleted, 5);
        compare(controller.storage.sources.length, 0);
    }
    function test_prune_rejects_earlier_storage_result() {
        controller.refreshStorage();
        var oldId = database.storageId;
        controller.prune();
        controller.exportFiles("/temporary", ["json"]);
        compare(database.prunes, 0);
        compare(database.exports, 0);
        database.maintenanceFinished(oldId, {ok: true, status: "success", sources: ["Old"]});
        controller.prune();
        database.maintenanceFinished(database.pruneId, {ok: true, status: "success", rowsDeleted: 5, sources: []});
        database.maintenanceFinished(oldId, {ok: true, status: "success", sources: ["Old"]});
        compare(controller.storage.sources.length, 0);
    }
    function test_storage_failure_is_unavailable() {
        controller.refreshStorage();
        database.maintenanceFinished(database.storageId, {ok: false, status: "failed", errorKey: "database-open-failed"});
        verify(!controller.storageAvailable);
        compare(controller.storage.errorKey, "database-open-failed");
    }
}
