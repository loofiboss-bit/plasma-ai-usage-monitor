import QtQuick

Item {
    id: controller

    enum LoadState {
        Idle,
        LoadingProbe,
        PluginUnavailable,
        PluginOlder,
        PluginNewer,
        LoadingRuntime,
        Ready,
        RuntimeUnavailable
    }

    required property string frontendVersion
    required property url probeSource
    required property url runtimeSource

    readonly property alias runtimeItem: runtimeLoader.item
    property int loadState: DependencyBootstrapController.Idle
    property string installedPluginVersion: ""
    property string errorDetail: ""
    property var probeComponent: null
    property var probeObject: null

    readonly property string stateName: {
        switch (loadState) {
        case DependencyBootstrapController.Idle:
            return "idle";
        case DependencyBootstrapController.LoadingProbe:
            return "loading-probe";
        case DependencyBootstrapController.PluginUnavailable:
            return "plugin-unavailable";
        case DependencyBootstrapController.PluginOlder:
            return "plugin-older";
        case DependencyBootstrapController.PluginNewer:
            return "plugin-newer";
        case DependencyBootstrapController.LoadingRuntime:
            return "loading-runtime";
        case DependencyBootstrapController.Ready:
            return "ready";
        case DependencyBootstrapController.RuntimeUnavailable:
            return "runtime-unavailable";
        }
        return "unknown";
    }

    visible: false
    width: 0
    height: 0

    function compareVersions(left, right) {
        var leftParts = left.split("-")[0].split(".");
        var rightParts = right.split("-")[0].split(".");
        var length = Math.max(leftParts.length, rightParts.length);

        for (var i = 0; i < length; ++i) {
            var leftPart = i < leftParts.length ? parseInt(leftParts[i], 10) : 0;
            var rightPart = i < rightParts.length ? parseInt(rightParts[i], 10) : 0;
            leftPart = isNaN(leftPart) ? 0 : leftPart;
            rightPart = isNaN(rightPart) ? 0 : rightPart;
            if (leftPart < rightPart)
                return -1;
            if (leftPart > rightPart)
                return 1;
        }

        return left === right ? 0 : (left < right ? -1 : 1);
    }

    function safeVersion(value) {
        var candidate = (value || "").toString().trim();
        return /^\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?$/.test(candidate)
            ? candidate : "unknown";
    }

    function supportReport() {
        var nativeVersion = installedPluginVersion !== ""
            ? safeVersion(installedPluginVersion) : "not_detected";
        return [
            "Plasma AI Usage Monitor Bootstrap Support Report",
            "Frontend version: " + safeVersion(frontendVersion),
            "Native plugin: " + nativeVersion,
            "Native status: " + stateName,
            "Sensitive error details, paths, identifiers, and credentials are omitted."
        ].join("\n");
    }

    function startProbe() {
        if (loadState !== DependencyBootstrapController.Idle)
            return;

        loadState = DependencyBootstrapController.LoadingProbe;
        probeComponent = Qt.createComponent(probeSource, Component.Asynchronous);
        handleProbeStatus();
    }

    function handleProbeStatus() {
        if (!probeComponent || probeComponent.status === Component.Loading)
            return;

        if (probeComponent.status === Component.Error) {
            errorDetail = probeComponent.errorString();
            loadState = DependencyBootstrapController.PluginUnavailable;
            return;
        }

        probeObject = probeComponent.createObject(controller);
        if (!probeObject || !probeObject.pluginVersion) {
            errorDetail = probeComponent.errorString();
            loadState = DependencyBootstrapController.PluginUnavailable;
            return;
        }

        installedPluginVersion = probeObject.pluginVersion;
        var comparison = compareVersions(installedPluginVersion, frontendVersion);
        if (comparison < 0) {
            loadState = DependencyBootstrapController.PluginOlder;
            return;
        }
        if (comparison > 0) {
            loadState = DependencyBootstrapController.PluginNewer;
            return;
        }

        loadState = DependencyBootstrapController.LoadingRuntime;
        runtimeLoader.active = true;
    }

    Connections {
        target: controller.probeComponent
        enabled: controller.probeComponent !== null

        function onStatusChanged() {
            controller.handleProbeStatus();
        }
    }

    Loader {
        id: runtimeLoader
        active: false
        asynchronous: false
        source: controller.runtimeSource

        onStatusChanged: {
            if (!active)
                return;
            if (status === Loader.Ready)
                controller.loadState = DependencyBootstrapController.Ready;
            else if (status === Loader.Error) {
                controller.errorDetail = "runtime-load-failed";
                controller.loadState = DependencyBootstrapController.RuntimeUnavailable;
            }
        }
    }

    Component.onCompleted: Qt.callLater(startProbe)
}
