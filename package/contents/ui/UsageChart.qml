pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

/**
 * Canvas-based line/area chart for visualizing usage history data.
 *
 * Expects chartData to be a QVariantList of QVariantMap with keys:
 *   timestamp, inputTokens, outputTokens, requestCount, cost, dailyCost,
 *   rlRequests, rlRequestsRemaining, rlTokens, rlTokensRemaining
 */
Item {
    id: chartRoot

    property var chartData: []
    property string provider: ""
    property string metric: "cost"  // "cost", "tokens", "requests", "rateLimit"
    property bool showMetricBar: true
    property bool showChartContent: true
    property bool showEmptyState: true
    property color lineColor: Kirigami.Theme.highlightColor
    property color areaColor: Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.15)
    property color gridColor: Qt.rgba(Kirigami.Theme.textColor.r,
                                       Kirigami.Theme.textColor.g,
                                       Kirigami.Theme.textColor.b, 0.1)
    readonly property bool hasEnoughData: !!chartData && chartData.length >= 2

    implicitHeight: Kirigami.Units.gridUnit * 10

    // Metric selector row
    RowLayout {
        id: metricBar
        visible: chartRoot.showMetricBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Kirigami.Units.smallSpacing

        Rectangle {
            Layout.alignment: Qt.AlignLeft
            color: Qt.alpha(Kirigami.Theme.textColor, 0.05)
            radius: Kirigami.Units.cornerRadius
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
            implicitHeight: Kirigami.Units.gridUnit * 1.5

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: [
                        { label: KI18n.i18n("Cost"), value: "cost" },
                        { label: KI18n.i18n("Tokens"), value: "tokens" },
                        { label: KI18n.i18n("Requests"), value: "requests" },
                        { label: KI18n.i18n("Rate Limit"), value: "rateLimit" }
                    ]

                    PlasmaComponents.ToolButton {
                        required property var modelData
                        text: modelData.label
                        checked: chartRoot.metric === modelData.value
                        down: checked
                        onClicked: {
                            chartRoot.metric = modelData.value;
                            canvas.requestPaint();
                        }
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        Layout.fillHeight: true
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }
    }

    // Empty state
    PlasmaComponents.Label {
        anchors.centerIn: parent
        visible: chartRoot.showEmptyState && !chartRoot.hasEnoughData
        text: KI18n.i18n("Not enough data to display chart")
        opacity: 0.5
        font.pointSize: Kirigami.Theme.smallFont.pointSize
    }

    // Hover state for tooltip
    property int hoveredIndex: -1
    property real hoverX: 0
    property real hoverY: 0

    // Cached chart geometry (set during paint, used by hover)
    property real chartMarginLeft: 50
    property real chartMarginRight: 10
    property real chartMarginTop: 10
    property real chartMarginBottom: 30

    Canvas {
        id: canvas
        anchors.top: metricBar.bottom
        anchors.topMargin: Kirigami.Units.smallSpacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: chartRoot.showChartContent && chartRoot.hasEnoughData

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();

            var data = chartRoot.chartData;
            if (!data || data.length < 2) return;

            var w = canvas.width;
            var h = canvas.height;
            var marginLeft = chartRoot.chartMarginLeft;
            var marginRight = chartRoot.chartMarginRight;
            var marginTop = chartRoot.chartMarginTop;
            var marginBottom = chartRoot.chartMarginBottom;
            var chartW = w - marginLeft - marginRight;
            var chartH = h - marginTop - marginBottom;

            if (chartW <= 0 || chartH <= 0) return;

            // Extract values based on selected metric
            var values = chartRoot.extractValues(data);
            if (values.length === 0) return;

            // Compute min/max — always start Y-axis at 0
            var minVal = 0;
            var maxVal = values[0];
            for (var i = 1; i < values.length; i++) {
                if (values[i] > maxVal) maxVal = values[i];
            }

            // Avoid flat line
            if (maxVal <= 0) {
                maxVal = 1;
            }

            // Add 10% padding to top
            maxVal += maxVal * 0.1;

            // ── Draw grid ──
            ctx.strokeStyle = chartRoot.gridColor;
            ctx.lineWidth = 1;
            ctx.font = "10px sans-serif";
            ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r,
                                     Kirigami.Theme.textColor.g,
                                     Kirigami.Theme.textColor.b, 0.5);
            ctx.textAlign = "right";

            var gridLines = 4;
            for (var g = 0; g <= gridLines; g++) {
                var gy = marginTop + chartH - (g / gridLines) * chartH;
                var gVal = minVal + (g / gridLines) * (maxVal - minVal);

                ctx.beginPath();
                ctx.moveTo(marginLeft, gy);
                ctx.lineTo(marginLeft + chartW, gy);
                ctx.stroke();

                ctx.fillText(chartRoot.formatValue(gVal), marginLeft - 5, gy + 4);
            }

            // ── Draw time labels on X axis ──
            ctx.textAlign = "center";
            var labelCount = Math.min(5, values.length);
            for (var lbl = 0; lbl < labelCount; lbl++) {
                var idx = Math.floor(lbl * (values.length - 1) / (labelCount - 1));
                var lx = marginLeft + (idx / (values.length - 1)) * chartW;
                var ts = data[idx].timestamp;
                ctx.fillText(chartRoot.formatTimestamp(ts), lx, h - 5);
            }

            // ── Compute points for Bézier and area ──
            var points = [];
            for (var c = 0; c < values.length; c++) {
                points.push({
                    x: marginLeft + (c / (values.length - 1)) * chartW,
                    y: marginTop + chartH - ((values[c] - minVal) / (maxVal - minVal)) * chartH
                });
            }

            // ── Draw area fill (with Bézier curves) ──
            ctx.beginPath();
            ctx.moveTo(points[0].x, marginTop + chartH);
            ctx.lineTo(points[0].x, points[0].y);
            for (var a = 1; a < points.length; a++) {
                var cp = chartRoot.bezierControlPoints(points, a - 1, 0.3);
                ctx.bezierCurveTo(cp.cp1x, cp.cp1y, cp.cp2x, cp.cp2y, points[a].x, points[a].y);
            }
            ctx.lineTo(points[points.length - 1].x, marginTop + chartH);
            ctx.closePath();
            ctx.fillStyle = chartRoot.areaColor;
            ctx.fill();

            // ── Draw line (with Bézier curves) ──
            ctx.beginPath();
            ctx.strokeStyle = chartRoot.lineColor;
            ctx.lineWidth = 2;
            ctx.lineJoin = "round";
            ctx.lineCap = "round";

            ctx.moveTo(points[0].x, points[0].y);
            for (var p = 1; p < points.length; p++) {
                var cp2 = chartRoot.bezierControlPoints(points, p - 1, 0.3);
                ctx.bezierCurveTo(cp2.cp1x, cp2.cp1y, cp2.cp2x, cp2.cp2y, points[p].x, points[p].y);
            }
            ctx.stroke();

            // ── Draw data points (if not too many) ──
            if (values.length <= 50) {
                ctx.fillStyle = chartRoot.lineColor;
                for (var d = 0; d < points.length; d++) {
                    ctx.beginPath();
                    ctx.arc(points[d].x, points[d].y, 3, 0, 2 * Math.PI);
                    ctx.fill();
                }
            }

            // ── Draw hover crosshair + highlight ──
            if (chartRoot.hoveredIndex >= 0 && chartRoot.hoveredIndex < points.length) {
                var hp = points[chartRoot.hoveredIndex];

                // Vertical crosshair line
                ctx.strokeStyle = Qt.rgba(Kirigami.Theme.textColor.r,
                                          Kirigami.Theme.textColor.g,
                                          Kirigami.Theme.textColor.b, 0.3);
                ctx.lineWidth = 1;
                ctx.setLineDash([4, 4]);
                ctx.beginPath();
                ctx.moveTo(hp.x, marginTop);
                ctx.lineTo(hp.x, marginTop + chartH);
                ctx.stroke();
                ctx.setLineDash([]);

                // Highlighted data point
                ctx.fillStyle = Kirigami.Theme.highlightColor;
                ctx.beginPath();
                ctx.arc(hp.x, hp.y, 5, 0, 2 * Math.PI);
                ctx.fill();
                ctx.strokeStyle = Kirigami.Theme.backgroundColor;
                ctx.lineWidth = 2;
                ctx.stroke();
            }
        }

        // Hover mouse area
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton

            onPositionChanged: function(mouse) {
                var data = chartRoot.chartData;
                if (!data || data.length < 2) return;

                var marginLeft = chartRoot.chartMarginLeft;
                var marginRight = chartRoot.chartMarginRight;
                var chartW = canvas.width - marginLeft - marginRight;
                if (chartW <= 0) return;

                var relX = mouse.x - marginLeft;
                if (relX < 0 || relX > chartW) {
                    if (chartRoot.hoveredIndex !== -1) {
                        chartRoot.hoveredIndex = -1;
                        canvas.requestPaint();
                    }
                    return;
                }

                var idx = Math.round((relX / chartW) * (data.length - 1));
                idx = Math.max(0, Math.min(idx, data.length - 1));
                if (idx !== chartRoot.hoveredIndex) {
                    chartRoot.hoveredIndex = idx;
                    chartRoot.hoverX = mouse.x;
                    chartRoot.hoverY = mouse.y;
                    canvas.requestPaint();
                }
            }

            onExited: {
                chartRoot.hoveredIndex = -1;
                canvas.requestPaint();
            }
        }

        // Tooltip popup
        Rectangle {
            id: hoverTooltip
            visible: chartRoot.hoveredIndex >= 0
            width: tooltipContent.implicitWidth + Kirigami.Units.smallSpacing * 2
            height: tooltipContent.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: 4
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.95)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.2)

            x: {
                var tx = chartRoot.hoverX + 12;
                if (tx + width > canvas.width) tx = chartRoot.hoverX - width - 12;
                return Math.max(0, tx);
            }
            y: {
                var ty = chartRoot.hoverY - height - 8;
                if (ty < 0) ty = chartRoot.hoverY + 12;
                return ty;
            }

            ColumnLayout {
                id: tooltipContent
                anchors.centerIn: parent
                spacing: 2

                PlasmaComponents.Label {
                    text: {
                        if (chartRoot.hoveredIndex < 0 || !chartRoot.chartData) return "";
                        var item = chartRoot.chartData[chartRoot.hoveredIndex];
                        if (!item) return "";
                        return chartRoot.formatTimestamp(item.timestamp);
                    }
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.bold: true
                }

                PlasmaComponents.Label {
                    text: {
                        if (chartRoot.hoveredIndex < 0 || !chartRoot.chartData) return "";
                        var item = chartRoot.chartData[chartRoot.hoveredIndex];
                        if (!item) return "";
                        var vals = chartRoot.extractValues([item]);
                        return chartRoot.formatValue(vals[0] || 0);
                    }
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: chartRoot.lineColor
                }
            }
        }

        // Repaint on resize
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // Repaint when data changes
    onChartDataChanged: canvas.requestPaint()
    onMetricChanged: canvas.requestPaint()

    // ── Helper functions ──

    // Compute Bézier control points for smooth curve between points[i] and points[i+1]
    function bezierControlPoints(points, i, tension) {
        var p0 = i > 0 ? points[i - 1] : points[i];
        var p1 = points[i];
        var p2 = points[i + 1];
        var p3 = (i + 2 < points.length) ? points[i + 2] : p2;

        var cp1x = p1.x + (p2.x - p0.x) * tension;
        var cp1y = p1.y + (p2.y - p0.y) * tension;
        var cp2x = p2.x - (p3.x - p1.x) * tension;
        var cp2y = p2.y - (p3.y - p1.y) * tension;

        return { cp1x: cp1x, cp1y: cp1y, cp2x: cp2x, cp2y: cp2y };
    }

    function extractValues(data) {
        var vals = [];
        for (var i = 0; i < data.length; i++) {
            var item = data[i];
            switch (chartRoot.metric) {
                case "cost":
                    vals.push(item.cost || 0);
                    break;
                case "tokens":
                    vals.push((item.inputTokens || 0) + (item.outputTokens || 0));
                    break;
                case "requests":
                    vals.push(item.requestCount || 0);
                    break;
                case "rateLimit":
                    // Show percentage of rate limit used
                    var total = item.rlRequests || 0;
                    var remaining = item.rlRequestsRemaining || 0;
                    if (total > 0) {
                        vals.push(((total - remaining) / total) * 100);
                    } else {
                        vals.push(0);
                    }
                    break;
                default:
                    vals.push(0);
            }
        }
        return vals;
    }

    function formatValue(val) {
        switch (chartRoot.metric) {
            case "cost":
                return Utils.formatMoney(val, chartData.length > 0 ? chartData[0].currency || "USD" : "USD");
            case "tokens":
                if (val >= 1000000) return (val / 1000000).toFixed(1) + "M";
                if (val >= 1000) return (val / 1000).toFixed(1) + "K";
                return Math.round(val).toString();
            case "requests":
                if (val >= 1000) return (val / 1000).toFixed(1) + "K";
                return Math.round(val).toString();
            case "rateLimit":
                return Math.round(val) + "%";
            default:
                return val.toFixed(1);
        }
    }

    function formatTimestamp(ts) {
        if (!ts) return "";
        var d;
        if (typeof ts === "string") {
            d = new Date(ts);
        } else {
            d = ts;
        }

        var now = new Date();
        var diffDays = Math.floor((now.getTime() - d.getTime()) / (24 * 60 * 60 * 1000));

        if (diffDays === 0) {
            // Today: show time
            return d.getHours().toString().padStart(2, '0') + ":" +
                   d.getMinutes().toString().padStart(2, '0');
        } else if (diffDays < 7) {
            // Within a week: show day name
            var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
            return days[d.getDay()];
        } else {
            // Older: show date
            return (d.getMonth() + 1) + "/" + d.getDate();
        }
    }
}
