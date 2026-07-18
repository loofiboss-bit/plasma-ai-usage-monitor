function prepareCaptureWindow(window) {
    if (!window || !window.normalWindow) {
        return;
    }

    const resourceClass = String(window.resourceClass || "").toLowerCase();
    const resourceName = String(window.resourceName || "").toLowerCase();
    const caption = String(window.caption || "");
    if (resourceClass.indexOf("plasmawindowed") < 0
            && resourceName.indexOf("plasmawindowed") < 0
            && caption !== "AI Usage Monitor Settings") {
        return;
    }

    const area = workspace.clientArea(KWin.MaximizeArea, window);
    window.frameGeometry = {
        x: area.x + Math.round(area.width * 0.08),
        y: area.y + Math.round(area.height * 0.06),
        width: Math.round(area.width * 0.84),
        height: Math.round(area.height * 0.88)
    };
    window.keepAbove = true;
    workspace.raiseWindow(window);
    workspace.activeWindow = window;
    print("V14_CAPTURE_WINDOW=" + window.internalId);
}

workspace.windowList().forEach(prepareCaptureWindow);
workspace.windowAdded.connect(prepareCaptureWindow);
