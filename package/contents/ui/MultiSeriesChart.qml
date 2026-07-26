pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

Item {
    id: chartRoot

    property var seriesData: []
    property string metric: "cost"
    property bool showEmptyState: true
    readonly property real legendChipMaxWidth: Kirigami.Units.gridUnit * 9
    readonly property bool showLegend: visibleSeries().length > 1

    // Hover state
    property bool hovering: false
    property real hoverX: 0
    property real hoverY: 0
    property real hoverTimeMs: 0
    property var hoverRows: []

    // Chart geometry
    property real marginLeft: Math.ceil(Math.max(
        Math.max(yAxisLabel0.width, yAxisLabel1.width, yAxisLabel2.width,
                 yAxisLabel3.width, yAxisLabel4.width)
            + Kirigami.Units.smallSpacing * 2,
        firstXAxisLabelMetrics.width / 2 + Kirigami.Units.smallSpacing))
    property real marginRight: Math.ceil(
        lastXAxisLabelMetrics.width / 2 + Kirigami.Units.smallSpacing)
    property real marginTop: 10
    property real marginBottom: Math.ceil(
        xAxisLabelMetrics.height + Kirigami.Units.smallSpacing * 2)

    TextMetrics {
        id: yAxisLabel0
        font.pixelSize: 10
        text: chartRoot.yAxisLabel(0)
    }
    TextMetrics {
        id: yAxisLabel1
        font.pixelSize: 10
        text: chartRoot.yAxisLabel(1)
    }
    TextMetrics {
        id: yAxisLabel2
        font.pixelSize: 10
        text: chartRoot.yAxisLabel(2)
    }
    TextMetrics {
        id: yAxisLabel3
        font.pixelSize: 10
        text: chartRoot.yAxisLabel(3)
    }
    TextMetrics {
        id: yAxisLabel4
        font.pixelSize: 10
        text: chartRoot.yAxisLabel(4)
    }
    TextMetrics {
        id: firstXAxisLabelMetrics
        font.pixelSize: 10
        text: chartRoot.firstXAxisLabel()
    }
    TextMetrics {
        id: lastXAxisLabelMetrics
        font.pixelSize: 10
        text: chartRoot.lastXAxisLabel()
    }
    TextMetrics {
        id: xAxisLabelMetrics
        font.pixelSize: 10
        text: chartRoot.firstXAxisLabel()
    }

    implicitHeight: Kirigami.Units.gridUnit * 11
    Accessible.role: Accessible.Graphic
    Accessible.name: accessibleSummary()

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: chartRoot.showLegend
                ? legendFlow.implicitHeight : 0
            visible: chartRoot.showLegend

            Flow {
                id: legendFlow
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: chartRoot.visibleSeries()

                    Rectangle {
                        id: legendChip
                        required property var modelData
                        radius: Kirigami.Units.cornerRadius
                        height: Kirigami.Units.gridUnit * 1.5
                        width: Math.min(chipLabel.implicitWidth + Kirigami.Units.smallSpacing * 4 + 16,
                                        chartRoot.legendChipMaxWidth)
                        color: Qt.alpha(legendChip.modelData.color, 0.10)
                        border.width: 1
                        border.color: Qt.alpha(legendChip.modelData.color, 0.3)

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: legendChip.modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: Kirigami.Units.smallSpacing
                        }

                        PlasmaComponents.Label {
                            id: chipLabel
                            text: legendChip.modelData.name
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: Kirigami.Units.smallSpacing * 2 + 8
                            anchors.right: parent.right
                            anchors.rightMargin: Kirigami.Units.smallSpacing
                            elide: Text.ElideRight
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Canvas {
                id: canvas
                anchors.fill: parent
                visible: chartRoot.hasData()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var parsed = chartRoot.parseSeries();
                    if (parsed.length === 0) return;

                    var bounds = chartRoot.computeBounds(parsed);
                    if (!bounds.valid) return;

                    var w = width;
                    var h = height;
                    var chartW = w - chartRoot.marginLeft - chartRoot.marginRight;
                    var chartH = h - chartRoot.marginTop - chartRoot.marginBottom;
                    if (chartW <= 0 || chartH <= 0) return;

                    var maxVal = bounds.maxValue;
                    if (maxVal <= 0) maxVal = 1;
                    maxVal *= 1.1;

                    // Grid + Y labels
                    ctx.strokeStyle = Qt.rgba(Kirigami.Theme.textColor.r,
                                              Kirigami.Theme.textColor.g,
                                              Kirigami.Theme.textColor.b, 0.12);
                    ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r,
                                            Kirigami.Theme.textColor.g,
                                            Kirigami.Theme.textColor.b, 0.55);
                    ctx.font = "10px sans-serif";
                    ctx.textAlign = "right";

                    var gridLines = 4;
                    for (var g = 0; g <= gridLines; g++) {
                        var ratio = g / gridLines;
                        var gy = chartRoot.marginTop + chartH - (ratio * chartH);
                        var val = ratio * maxVal;

                        ctx.beginPath();
                        ctx.moveTo(chartRoot.marginLeft, gy);
                        ctx.lineTo(chartRoot.marginLeft + chartW, gy);
                        ctx.stroke();

                        ctx.fillText(chartRoot.formatMetricValue(val), chartRoot.marginLeft - 6, gy + 3);
                    }

                    // X labels
                    ctx.textAlign = "center";
                    var xTicks = 4;
                    for (var t = 0; t <= xTicks; t++) {
                        var tr = t / xTicks;
                        var tx = chartRoot.marginLeft + tr * chartW;
                        var tm = bounds.minTime + tr * (bounds.maxTime - bounds.minTime);
                        ctx.fillText(chartRoot.formatAxisTime(tm), tx, h - 5);
                    }

                    // Draw series
                    for (var s = 0; s < parsed.length; s++) {
                        var series = parsed[s];
                        if (!series.points || series.points.length === 0) continue;

                        ctx.beginPath();
                        ctx.strokeStyle = series.color;
                        ctx.lineWidth = 2;
                        ctx.lineJoin = "round";
                        ctx.lineCap = "round";

                        var drawing = false;
                        var drawnPoints = 0;
                        var singlePoint = null;
                        for (var p = 0; p < series.points.length; p++) {
                            var point = series.points[p];
                            if (point.gap) {
                                drawing = false;
                                continue;
                            }
                            var px = chartRoot.marginLeft + ((point.t - bounds.minTime) / (bounds.maxTime - bounds.minTime || 1)) * chartW;
                            var py = chartRoot.marginTop + chartH - (point.v / maxVal) * chartH;
                            point.x = px;
                            point.y = py;
                            drawnPoints++;
                            singlePoint = point;

                            if (!drawing) {
                                ctx.moveTo(px, py);
                                drawing = true;
                            } else {
                                ctx.lineTo(px, py);
                            }
                        }
                        ctx.stroke();
                        if (drawnPoints === 1 && singlePoint) {
                            ctx.fillStyle = series.color;
                            ctx.beginPath();
                            ctx.arc(singlePoint.x, singlePoint.y, 3, 0,
                                    Math.PI * 2);
                            ctx.fill();
                        }
                    }

                    if (chartRoot.hovering) {
                        var clampedX = Math.max(chartRoot.marginLeft, Math.min(chartRoot.hoverX, chartRoot.marginLeft + chartW));

                        ctx.strokeStyle = Qt.rgba(Kirigami.Theme.textColor.r,
                                                  Kirigami.Theme.textColor.g,
                                                  Kirigami.Theme.textColor.b, 0.35);
                        ctx.lineWidth = 1;
                        ctx.setLineDash([4, 4]);
                        ctx.beginPath();
                        ctx.moveTo(clampedX, chartRoot.marginTop);
                        ctx.lineTo(clampedX, chartRoot.marginTop + chartH);
                        ctx.stroke();
                        ctx.setLineDash([]);

                        // Highlight nearest points
                        var rows = chartRoot.nearestRows(parsed, chartRoot.hoverTimeMs);
                        for (var r = 0; r < rows.length; r++) {
                            var row = rows[r];
                            if (!row.point) continue;
                            ctx.fillStyle = row.color;
                            ctx.beginPath();
                            ctx.arc(row.point.x, row.point.y, 4, 0, Math.PI * 2);
                            ctx.fill();
                            ctx.strokeStyle = Kirigami.Theme.backgroundColor;
                            ctx.lineWidth = 1.5;
                            ctx.stroke();
                        }
                    }
                }

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }

            PlasmaComponents.Label {
                anchors.centerIn: parent
                visible: chartRoot.showEmptyState && !chartRoot.hasData()
                text: KI18n.i18n("No comparison data available for this range")
                opacity: 0.55
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }

            MouseArea {
                anchors.fill: canvas
                visible: chartRoot.hasData()
                hoverEnabled: true
                acceptedButtons: Qt.NoButton

                onPositionChanged: function(mouse) {
                    var bounds = chartRoot.computeBounds(chartRoot.parseSeries());
                    if (!bounds.valid) return;

                    var chartW = canvas.width - chartRoot.marginLeft - chartRoot.marginRight;
                    if (chartW <= 0) return;

                    if (mouse.x < chartRoot.marginLeft || mouse.x > chartRoot.marginLeft + chartW
                        || mouse.y < chartRoot.marginTop || mouse.y > canvas.height - chartRoot.marginBottom) {
                        if (chartRoot.hovering) {
                            chartRoot.hovering = false;
                            chartRoot.hoverRows = [];
                            canvas.requestPaint();
                        }
                        return;
                    }

                    chartRoot.hovering = true;
                    chartRoot.hoverX = mouse.x;
                    chartRoot.hoverY = mouse.y;
                    var ratio = (mouse.x - chartRoot.marginLeft) / chartW;
                    chartRoot.hoverTimeMs = bounds.minTime + ratio * (bounds.maxTime - bounds.minTime);
                    chartRoot.hoverRows = chartRoot.nearestRows(chartRoot.parseSeries(), chartRoot.hoverTimeMs);
                    canvas.requestPaint();
                }

                onExited: {
                    chartRoot.hovering = false;
                    chartRoot.hoverRows = [];
                    canvas.requestPaint();
                }
            }

            Rectangle {
                id: tooltip
                visible: chartRoot.hovering && chartRoot.hoverRows.length > 0
                radius: 4
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.96)
                border.width: 1
                border.color: Qt.alpha(Kirigami.Theme.textColor, 0.2)
                z: 20

                width: Math.min(chartRoot.width * 0.86,
                                tooltipColumn.implicitWidth + Kirigami.Units.smallSpacing * 2)
                height: tooltipColumn.implicitHeight + Kirigami.Units.smallSpacing * 2

                x: {
                    var tx = chartRoot.hoverX + 12;
                    if (tx + width > canvas.width) tx = chartRoot.hoverX - width - 12;
                    return Math.max(0, tx);
                }
                y: {
                    var ty = chartRoot.hoverY - height - 10;
                    if (ty < 0) ty = chartRoot.hoverY + 12;
                    return ty;
                }

                ColumnLayout {
                    id: tooltipColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    spacing: 4

                    PlasmaComponents.Label {
                        text: chartRoot.formatAxisTime(chartRoot.hoverTimeMs)
                        font.bold: true
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }

                    PlasmaComponents.Label {
                        text: chartRoot.metricLabel()
                        opacity: 0.65
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }

                    Repeater {
                        model: chartRoot.hoverRows

                        RowLayout {
                            id: hoverRow
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: hoverRow.modelData.color
                            }

                            PlasmaComponents.Label {
                                text: hoverRow.modelData.name
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }

                            PlasmaComponents.Label {
                                text: chartRoot.formatMetricValue(
                                    hoverRow.modelData.value)
                                font.bold: true
                                horizontalAlignment: Text.AlignRight
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }
                    }
                }
            }
        }
    }

    onSeriesDataChanged: canvas.requestPaint()
    onMetricChanged: canvas.requestPaint()

    function hasData() {
        var series = seriesData || [];
        for (var i = 0; i < series.length; i++) {
            var points = series[i].points || [];
            for (var p = 0; p < points.length; p++) {
                if (points[p].available !== false
                        && points[p].value !== null
                        && points[p].value !== undefined
                        && Number.isFinite(Number(points[p].value))) return true;
            }
        }
        return false;
    }

    function accessibleSummary() {
        var visible = chartRoot.visibleSeries();
        if (visible.length === 0)
            return KI18n.i18n("%1 chart. No compatible data.", chartRoot.metricLabel());
        var descriptions = [];
        for (var i = 0; i < visible.length; ++i) {
            var points = visible[i].points || [];
            var available = 0;
            var gaps = 0;
            for (var p = 0; p < points.length; ++p) {
                if (points[p].available === false
                        || points[p].value === null
                        || points[p].value === undefined) {
                    gaps++;
                } else if (Number.isFinite(Number(points[p].value))) {
                    available++;
                }
            }
            descriptions.push(KI18n.i18n("%1: %2 recorded points, %3 gaps",
                                   visible[i].name, available, gaps));
        }
        return KI18n.i18n("%1 chart. %2", chartRoot.metricLabel(), descriptions.join(KI18n.i18n(" · ")));
    }

    function visibleSeries() {
        var series = seriesData || [];
        var out = [];
        for (var i = 0; i < series.length; i++) {
            var points = series[i].points || [];
            var available = points.some(function(point) {
                return point.available !== false
                    && point.value !== null
                    && point.value !== undefined
                    && Number.isFinite(Number(point.value));
            });
            if (available) {
                out.push({
                    name: series[i].name || KI18n.i18n("Series %1", i + 1),
                    color: series[i].color || paletteColor(i),
                    points: points
                });
            }
        }
        return out;
    }

    function parseSeries() {
        var src = seriesData || [];
        var parsed = [];

        for (var i = 0; i < src.length; i++) {
            var s = src[i];
            var input = s.points || [];
            var pts = [];

            for (var p = 0; p < input.length; p++) {
                var ts = input[p].timestamp;
                var d = (typeof ts === "string") ? new Date(ts) : ts;
                if (!d || isNaN(d.getTime())) continue;
                var available = input[p].available !== false
                    && input[p].value !== null
                    && input[p].value !== undefined
                    && Number.isFinite(Number(input[p].value));
                pts.push({
                    t: d.getTime(),
                    v: available ? Number(input[p].value) : 0,
                    gap: !available,
                    x: 0,
                    y: 0
                });
            }

            pts.sort(function(a, b) { return a.t - b.t; });
            if (pts.length > 0) {
                parsed.push({
                    name: s.name || KI18n.i18n("Series %1", i + 1),
                    color: s.color || paletteColor(i),
                    points: pts
                });
            }
        }

        return parsed;
    }

    function computeBounds(parsed) {
        if (!parsed || parsed.length === 0) {
            return { valid: false };
        }

        var minTime = Number.MAX_SAFE_INTEGER;
        var maxTime = 0;
        var maxValue = 0;

        for (var i = 0; i < parsed.length; i++) {
            var pts = parsed[i].points;
            for (var p = 0; p < pts.length; p++) {
                if (pts[p].gap) continue;
                if (pts[p].t < minTime) minTime = pts[p].t;
                if (pts[p].t > maxTime) maxTime = pts[p].t;
                if (pts[p].v > maxValue) maxValue = pts[p].v;
            }
        }

        if (minTime === Number.MAX_SAFE_INTEGER) {
            return { valid: false };
        }

        if (maxTime <= minTime) {
            maxTime = minTime + 60 * 1000;
        }

        return {
            valid: true,
            minTime: minTime,
            maxTime: maxTime,
            maxValue: maxValue
        };
    }

    function axisMaximum() {
        var bounds = computeBounds(parseSeries());
        if (!bounds.valid) return 1;
        var maximum = bounds.maxValue > 0 ? bounds.maxValue : 1;
        return maximum * 1.1;
    }

    function yAxisLabel(tick) {
        return formatMetricValue(axisMaximum() * tick / 4);
    }

    function firstXAxisLabel() {
        var bounds = computeBounds(parseSeries());
        return bounds.valid ? formatAxisTime(bounds.minTime) : "";
    }

    function lastXAxisLabel() {
        var bounds = computeBounds(parseSeries());
        return bounds.valid ? formatAxisTime(bounds.maxTime) : "";
    }

    function nearestRows(parsed, hoverTime) {
        var rows = [];

        for (var i = 0; i < parsed.length; i++) {
            var pts = parsed[i].points.filter(function(point) {
                return !point.gap;
            });
            if (pts.length === 0) continue;

            var nearest = pts[0];
            var bestDist = Math.abs(pts[0].t - hoverTime);
            for (var p = 1; p < pts.length; p++) {
                var dist = Math.abs(pts[p].t - hoverTime);
                if (dist < bestDist) {
                    bestDist = dist;
                    nearest = pts[p];
                }
            }

            rows.push({
                name: parsed[i].name,
                color: parsed[i].color,
                value: nearest.v,
                point: nearest
            });
        }

        rows.sort(function(a, b) {
            return b.value - a.value;
        });

        return rows;
    }

    function paletteColor(index) {
        var colors = [
            "#10A37F", "#4285F4", "#F55036", "#D4A574", "#5B6EE1", "#FF7000", "#1DA1F2", "#6e40c9"
        ];
        return colors[index % colors.length];
    }

    function formatMetricValue(value) {
        if (metric === "cost") {
            var currencies = {};
            for (var i = 0; i < seriesData.length; i++) {
                currencies[seriesData[i].currency || "USD"] = true;
                if (seriesData[i].mixedCurrency) currencies.__mixed = true;
            }
            var keys = Object.keys(currencies).filter(function(key) { return key !== "__mixed"; });
            return keys.length === 1 && !currencies.__mixed
                ? Utils.formatMoney(value, keys[0])
                : value.toFixed(value < 1 ? 4 : 2) + " " + KI18n.i18n("mixed currencies");
        }
        if (metric === "tokens") {
            if (value >= 1000000) return (value / 1000000).toFixed(1) + "M";
            if (value >= 1000) return (value / 1000).toFixed(1) + "K";
            return Math.round(value).toString();
        }
        if (metric === "requests" || metric === "usageCount" || metric === "remaining") {
            if (value >= 1000) return (value / 1000).toFixed(1) + "K";
            return Math.round(value).toString();
        }
        return Math.round(value) + "%";
    }

    function metricLabel() {
        switch (metric) {
            case "cost": return KI18n.i18n("Cost");
            case "tokens": return KI18n.i18n("Tokens");
            case "requests": return KI18n.i18n("Requests");
            case "rateLimitUsed": return KI18n.i18n("Rate Limit Used");
            case "percentUsed": return KI18n.i18n("Percent Used");
            case "usageCount": return KI18n.i18n("Usage Count");
            case "remaining": return KI18n.i18n("Remaining");
            default: return KI18n.i18n("Value");
        }
    }

    function formatAxisTime(ms) {
        var d = (typeof ms === "number") ? new Date(ms) : ms;
        if (!d || isNaN(d.getTime())) return "";

        var bounds = chartRoot.computeBounds(chartRoot.parseSeries());
        var span = bounds.valid ? bounds.maxTime - bounds.minTime : 0;
        if (span <= 2 * 86400000) return Qt.formatDateTime(d, "MMM d hh:mm");
        if (span <= 14 * 86400000) return Qt.formatDateTime(d, "ddd MMM d");
        return Qt.formatDate(d, "MMM d");
    }
}
