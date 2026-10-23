function severity(state) {
    switch (state) {
    case "reporting_actual":
    case "reporting_estimate":
    case "connected_connectivity_only": return "positive";
    case "needs_configuration":
    case "unavailable_locally":
    case "degraded": return "warning";
    case "failed": return "error";
    default: return "information";
    }
}

function message(source) {
    var state = source.readinessStateKey || "";
    switch (state) {
    case "reporting_actual":
        return i18n("Verification succeeded and returned supported usage or spend data.");
    case "reporting_estimate":
        return i18n("Verification succeeded and returned estimated or local activity data.");
    case "connected_connectivity_only":
        return i18n("Verification succeeded. This source confirms connectivity only.");
    case "disabled":
        return i18n("This source is disabled. Enable it to verify monitoring.");
    case "unavailable_locally":
        return source.nextActionText || i18n("The local tool is not available. Install it, then verify again.");
    case "needs_configuration":
        return source.nextActionText || i18n("Complete the source configuration, then verify again.");
    case "ready_to_verify":
        return i18n("This source is ready for verification. Run Verify to check it.");
    case "verifying":
        return i18n("Running the safe read-only scheduled check…");
    case "waiting_for_activity":
        return i18n("The local tool was detected. Use it to record activity, then verify again.");
    case "degraded":
        return source.nextActionText || i18n("Verification completed with degraded data.");
    case "failed":
        return source.nextActionText || i18n("Verification failed. Review the source configuration and retry.");
    default:
        return i18n("Verification status is unavailable. Run Verify to check this source again.");
    }
}
