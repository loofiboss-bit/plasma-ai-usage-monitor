.pragma library

function dateMilliseconds(value) {
    if (value === undefined || value === null || value === "") return 0;
    var date = value instanceof Date ? value : new Date(value);
    var milliseconds = date.getTime();
    return Number.isFinite(milliseconds) ? milliseconds : 0;
}

function retryBlocked(retryAfter, nowMs) {
    var retryAt = dateMilliseconds(retryAfter);
    var currentTime = nowMs === undefined ? Date.now() : nowMs;
    return retryAt > currentTime;
}

function canRequestAction(retryAfter, nowMs) {
    return !retryBlocked(retryAfter, nowMs);
}
