pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: quotaStack

    required property var monitor
    property color accentColor: Kirigami.Theme.highlightColor
    property bool collapsed: false
    readonly property var visibleRows: filteredRows()
    readonly property var primaryRow: mostConstrainedRow()

    spacing: Kirigami.Units.smallSpacing

    Repeater {
        model: quotaStack.collapsed ? [] : quotaStack.visibleRows
        delegate: QuotaRow {
            required property var modelData
            Layout.fillWidth: true
            rowData: modelData
            accentColor: quotaStack.accentColor
        }
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        visible: !quotaStack.collapsed && quotaStack.visibleRows.length === 0
        text: KI18n.i18n("No quota data")
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        opacity: 0.6
        elide: Text.ElideRight
    }

    function filteredRows() {
        var rows = monitor?.quotaWindows || [];
        var result = [];
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].visibleByDefault !== false) {
                result.push(rows[i]);
            }
        }
        return result;
    }

    function mostConstrainedRow() {
        var rows = visibleRows;
        var best = null;
        var bestScore = -1;
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            var score = row.percentUsed !== undefined ? row.percentUsed : -1;
            if (row.badge === "Needs review" && score < 0) score = 1;
            if (!best || score > bestScore) {
                best = row;
                bestScore = score;
            }
        }
        return best;
    }

    function summaryText() {
        var row = primaryRow;
        if (!row) return "";
        if (row.used !== undefined && row.limit !== undefined && row.limit > 0) {
            return row.used + "/" + row.limit;
        }
        if (row.percentUsed !== undefined) {
            return Math.round(row.percentUsed) + "%";
        }
        if (row.limit !== undefined) {
            return row.unit === "usage_multiplier" ? row.limit + "x" : row.limit + "";
        }
        return row.badge || "";
    }
}
