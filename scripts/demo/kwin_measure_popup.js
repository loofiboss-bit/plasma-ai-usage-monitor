function reportWindow(eventName, window) {
    if (!window) return;
    print("AI_USAGE_POPUP event=" + eventName
          + " pid=" + String(window.pid || 0)
          + " id=" + String(window.internalId || "")
          + " caption=" + JSON.stringify(String(window.caption || ""))
          + " resource=" + JSON.stringify(String(window.resourceName || "")));
}

workspace.windowAdded.connect(function(window) {
    reportWindow("added", window);
});
workspace.windowRemoved.connect(function(window) {
    reportWindow("removed", window);
});
