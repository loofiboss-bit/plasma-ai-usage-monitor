import QtQuick
import org.kde.kirigami as Kirigami

/**
 * A GitHub-style activity heatmap for AI usage history.
 * Displays a 52-week grid of daily activity intensity.
 */
Canvas {
    id: heatmap
    
    property var activityData: [] // List of { date: "YYYY-MM-DD", value: ... }
    property double maxIntensity: 1.0
    property color baseColor: Kirigami.Theme.highlightColor
    readonly property color emptyColor: Qt.rgba(Kirigami.Theme.textColor.r,
                                                 Kirigami.Theme.textColor.g,
                                                 Kirigami.Theme.textColor.b, 0.08)
    property int cellSize: 12
    property int spacing: 2
    property int rows: 7
    property int columns: 53 // 52 weeks + 1
    
    implicitWidth: columns * (cellSize + spacing) + spacing
    implicitHeight: rows * (cellSize + spacing) + spacing
    
    signal hovered(string date, var value, bool recorded)
    signal clicked(string date)

    onActivityDataChanged: requestPaint()
    onMaxIntensityChanged: requestPaint()
    onBaseColorChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        
        var today = new Date();
        // Start from 365 days ago
        var startDate = new Date(today);
        startDate.setDate(today.getDate() - 364);
        // Adjust to the start of that week (Sunday or Monday depending on locale, let's use Sunday)
        var dayOfWeek = startDate.getDay();
        startDate.setDate(startDate.getDate() - dayOfWeek);

        var dataMap = {};
        for (var i = 0; i < activityData.length; i++) {
            dataMap[activityData[i].date] = activityData[i].value;
        }

        for (var col = 0; col < columns; col++) {
            for (var row = 0; row < rows; row++) {
                var currentDate = new Date(startDate);
                currentDate.setDate(startDate.getDate() + (col * 7) + row);
                
                var dateStr = currentDate.toISOString().split('T')[0];
                var recorded = Object.prototype.hasOwnProperty.call(dataMap, dateStr);
                var value = recorded ? Number(dataMap[dateStr]) : 0;
                
                var x = col * (cellSize + spacing) + spacing;
                var y = row * (cellSize + spacing) + spacing;
                
                // Draw square
                ctx.fillStyle = getIntensityColor(value, recorded);
                
                // Rounded corners for modern look
                var r = 2;
                ctx.beginPath();
                ctx.moveTo(x + r, y);
                ctx.lineTo(x + cellSize - r, y);
                ctx.quadraticCurveTo(x + cellSize, y, x + cellSize, y + r);
                ctx.lineTo(x + cellSize, y + cellSize - r);
                ctx.quadraticCurveTo(x + cellSize, y + cellSize, x + cellSize - r, y + cellSize);
                ctx.lineTo(x + r, y + cellSize);
                ctx.quadraticCurveTo(x, y + cellSize, x, y + cellSize - r);
                ctx.lineTo(x, y + r);
                ctx.quadraticCurveTo(x, y, x + r, y);
                ctx.closePath();
                ctx.fill();
                
                // Dim future dates or dates beyond "today"
                if (currentDate > today) {
                    ctx.fillStyle = Qt.rgba(Kirigami.Theme.backgroundColor.r, 
                                            Kirigami.Theme.backgroundColor.g, 
                                            Kirigami.Theme.backgroundColor.b, 0.5);
                    ctx.fill();
                }
            }
        }
    }

    function getIntensityColor(value, recorded) {
        if (!recorded || value <= 0) return emptyColor;
        
        var ratio = maxIntensity > 0 ? (value / maxIntensity) : 0;
        // 5-step scale
        var alpha = 0.2;
        if (ratio > 0.8) alpha = 1.0;
        else if (ratio > 0.6) alpha = 0.8;
        else if (ratio > 0.4) alpha = 0.6;
        else if (ratio > 0.2) alpha = 0.4;
        
        return Qt.rgba(baseColor.r, baseColor.g, baseColor.b, alpha);
    }

    function valueForDate(date) {
        for (var i = 0; i < activityData.length; i++) {
            if (activityData[i].date === date)
                return { recorded: true, value: Number(activityData[i].value) };
        }
        return { recorded: false, value: undefined };
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        
        onPositionChanged: (mouse) => {
            var col = Math.floor((mouse.x - heatmap.spacing) / (heatmap.cellSize + heatmap.spacing));
            var row = Math.floor((mouse.y - heatmap.spacing) / (heatmap.cellSize + heatmap.spacing));
            
            if (col >= 0 && col < heatmap.columns && row >= 0 && row < heatmap.rows) {
                var today = new Date();
                var startDate = new Date(today);
                startDate.setDate(today.getDate() - 364);
                var dayOfWeek = startDate.getDay();
                startDate.setDate(startDate.getDate() - dayOfWeek);
                
                var targetDate = new Date(startDate);
                targetDate.setDate(startDate.getDate() + (col * 7) + row);
                var dateStr = targetDate.toISOString().split('T')[0];
                
                var point = heatmap.valueForDate(dateStr);
                
                if (targetDate <= today) {
                    heatmap.hovered(dateStr, point.value, point.recorded);
                }
            }
        }
        
        onClicked: (mouse) => {
            var col = Math.floor((mouse.x - heatmap.spacing) / (heatmap.cellSize + heatmap.spacing));
            var row = Math.floor((mouse.y - heatmap.spacing) / (heatmap.cellSize + heatmap.spacing));
            
            if (col >= 0 && col < heatmap.columns && row >= 0 && row < heatmap.rows) {
                var today = new Date();
                var startDate = new Date(today);
                startDate.setDate(today.getDate() - 364);
                var dayOfWeek = startDate.getDay();
                startDate.setDate(startDate.getDate() - dayOfWeek);
                
                var targetDate = new Date(startDate);
                targetDate.setDate(startDate.getDate() + (col * 7) + row);
                var dateStr = targetDate.toISOString().split('T')[0];
                
                if (targetDate <= today) {
                    heatmap.clicked(dateStr);
                }
            }
        }
    }
}
