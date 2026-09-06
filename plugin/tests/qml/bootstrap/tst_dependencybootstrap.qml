import QtQuick
import QtQuick.Layouts
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

    Component {
        id: bootstrapFixture
        Item { property string marker: "bootstrap" }
    }

    Component {
        id: runtimeFixture
        Item {
            property string marker: "runtime"
            Layout.minimumWidth: 120
            Layout.preferredWidth: 180
            Layout.maximumWidth: 240
            Layout.minimumHeight: 24
            Layout.preferredHeight: 36
            Layout.maximumHeight: 48
            Layout.fillWidth: false
            Layout.fillHeight: true
        }
    }

    Component {
        id: representationLoaderComponent
        AppUi.RuntimeRepresentationLoader {}
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
        verify(controller.supportReport().indexOf("Native plugin: not_detected") >= 0);
        verify(controller.supportReport().indexOf("Native status: plugin-unavailable") >= 0);
        verify(controller.supportReport().indexOf(controller.errorDetail) < 0);
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
        verify(controller.supportReport().indexOf("Frontend version: 13.0.0") >= 0);
        verify(controller.supportReport().indexOf("Native plugin: 12.9.0") >= 0);
        verify(controller.supportReport().indexOf("Native status: plugin-older") >= 0);
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

    function test_representationSwapsWhenRuntimeBecomesReady() {
        var loader = representationLoaderComponent.createObject(testCase, {
            runtimeReady: false,
            bootstrapRepresentation: bootstrapFixture,
            runtimeRepresentation: runtimeFixture
        });
        verify(loader !== null);
        activeControllers.push(loader);

        tryCompare(loader.item, "marker", "bootstrap");
        loader.runtimeReady = true;
        tryCompare(loader.item, "marker", "runtime");
        compare(loader.Layout.minimumWidth, 120);
        compare(loader.Layout.preferredWidth, 180);
        compare(loader.Layout.maximumWidth, 240);
        compare(loader.Layout.minimumHeight, 24);
        compare(loader.Layout.preferredHeight, 36);
        compare(loader.Layout.maximumHeight, 48);
        compare(loader.Layout.fillWidth, false);
        compare(loader.Layout.fillHeight, true);

        loader.item.Layout.preferredWidth = 220;
        compare(loader.Layout.preferredWidth, 220);
        loader.runtimeReady = false;
        tryCompare(loader.item, "marker", "bootstrap");
        compare(loader.Layout.preferredWidth, -1);
        compare(loader.Layout.maximumWidth, Infinity);
    }
}
