function prepareCaptureWindow(window) {
    if (!window || !window.normalWindow) return;

    const resourceClass = String(window.resourceClass || "").toLowerCase();
    const resourceName = String(window.resourceName || "").toLowerCase();
    const caption = String(window.caption || "");
    if (resourceClass.indexOf("plasmawindowed") < 0
            && resourceName.indexOf("plasmawindowed") < 0
            && caption !== "AI Usage Monitor Settings") return;

    const area = workspace.clientArea(KWin.MaximizeArea, window);
    window.frameGeometry = {
        x: area.x + Math.round((area.width - 1600) / 2),
        y: area.y + Math.round((area.height - 900) / 2),
        width: 1600,
        height: 900
    };
    window.keepAbove = true;
    workspace.raiseWindow(window);
    workspace.activeWindow = window;
    print("V18_CAPTURE_WINDOW=" + window.internalId);
}

workspace.windowList().forEach(prepareCaptureWindow);
workspace.windowAdded.connect(prepareCaptureWindow);
