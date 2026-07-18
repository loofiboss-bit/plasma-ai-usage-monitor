import QtQuick
import QtTest
import "../../../../package/contents/ui" as AppUi

TestCase {
    id: testCase
    name: "DependencyBootstrap"

    property var activeControllers: []

    Component {
        id: controllerComponent
        AppUi.DependencyBootstrapController {}
    }

    function cleanup() {
        for (var i = 0; i < activeControllers.length; ++i)
            activeControllers[i].destroy();
        activeControllers = [];
    }

    function createController(probeFixture, runtimeFixture) {
        var controller = controllerComponent.createObject(testCase, {
            frontendVersion: "13.0.0",
            probeSource: Qt.resolvedUrl("fixtures/" + probeFixture),
            runtimeSource: Qt.resolvedUrl("fixtures/" + (runtimeFixture || "RuntimeReady.qml"))
        });
        verify(controller !== null);
        activeControllers.push(controller);
        return controller;
    }

    function test_missingPluginShowsRecoveryState() {
        var controller = createController("ProbeMissing.qml");

        tryCompare(controller, "stateName", "plugin-unavailable");
        compare(controller.installedPluginVersion, "");
        compare(controller.runtimeItem, null);
    }

    function test_matchingPluginLoadsRuntime() {
        var controller = createController("ProbeMatching.qml");

        tryCompare(controller, "stateName", "ready");
        compare(controller.installedPluginVersion, "13.0.0");
        verify(controller.runtimeItem !== null);
        verify(controller.runtimeItem.initialized);
    }

    function test_olderPluginStopsBeforeRuntime() {
        var controller = createController("ProbeOlder.qml");

        tryCompare(controller, "stateName", "plugin-older");
        compare(controller.installedPluginVersion, "12.9.0");
        compare(controller.runtimeItem, null);
    }

    function test_newerPluginStopsBeforeRuntime() {
        var controller = createController("ProbeNewer.qml");

        tryCompare(controller, "stateName", "plugin-newer");
        compare(controller.installedPluginVersion, "14.1.0");
        compare(controller.runtimeItem, null);
    }

    function test_matchingButUnusablePluginShowsRecoveryState() {
        var controller = createController("ProbeMatching.qml", "RuntimeMissing.qml");

        tryCompare(controller, "stateName", "runtime-unavailable");
        compare(controller.installedPluginVersion, "13.0.0");
        compare(controller.runtimeItem, null);
    }
}
