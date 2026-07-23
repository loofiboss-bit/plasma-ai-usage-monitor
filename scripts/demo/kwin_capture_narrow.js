function prepareCaptureWindow(window) {
    if (!window || !window.normalWindow) {
        return;
    }

    const resourceClass = String(window.resourceClass || "").toLowerCase();
    const resourceName = String(window.resourceName || "").toLowerCase();
    if (resourceClass.indexOf("plasmawindowed") < 0
            && resourceName.indexOf("plasmawindowed") < 0) {
        return;
    }

    const area = workspace.clientArea(KWin.MaximizeArea, window);
    window.frameGeometry = {
        x: area.x + Math.round((area.width - 640) / 2),
        y: area.y + Math.round((area.height - 860) / 2),
        width: 640,
        height: 860
    };
    window.keepAbove = true;
    workspace.raiseWindow(window);
    workspace.activeWindow = window;
    print("V15_NARROW_CAPTURE_WINDOW=" + window.internalId);
}

workspace.windowList().forEach(prepareCaptureWindow);
workspace.windowAdded.connect(prepareCaptureWindow);
