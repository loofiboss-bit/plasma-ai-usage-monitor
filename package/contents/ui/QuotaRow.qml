import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

ColumnLayout {
    id: quotaRow

    required property var rowData
    property color accentColor: Kirigami.Theme.highlightColor

    spacing: Kirigami.Units.smallSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: quotaRow.rowData?.label || i18n("Quota")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            elide: Text.ElideRight
        }

        SourceBadge {
            text: quotaRow.rowData?.badge || i18n("Unknown")
        }

        PlasmaComponents.Label {
            text: quotaRow.valueText()
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            font.bold: true
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
    }

    QQC2.ProgressBar {
        id: quotaProgress
        Layout.fillWidth: true
        Layout.preferredHeight: 6
        visible: quotaRow.hasProgress()
        from: 0
        to: 100
        value: Math.min(100, Math.max(0, quotaRow.rowData?.percentUsed || 0))
        background: Rectangle {
            implicitHeight: 6
            radius: 3
            color: Qt.alpha(Kirigami.Theme.textColor, 0.1)
        }
        contentItem: Item {
            implicitHeight: 6

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: quotaProgress.visualPosition * parent.width
                height: 6
                radius: 3
                color: Utils.usageColor(quotaRow.rowData?.percentUsed || 0, Kirigami.Theme)
                Behavior on width { NumberAnimation { duration: 180 } }
            }
        }
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        visible: (quotaRow.rowData?.timeUntilReset || "").length > 0
        text: i18n("Resets in %1", quotaRow.rowData?.timeUntilReset || "")
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        opacity: 0.55
        elide: Text.ElideRight
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        visible: (quotaRow.rowData?.availabilityReason || "").length > 0
        text: quotaRow.rowData?.availabilityReason || ""
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: Kirigami.Theme.neutralTextColor
        wrapMode: Text.Wrap
    }

    function hasProgress() {
        return rowData && rowData.percentUsed !== undefined && rowData.percentUsed !== null;
    }

    function valueText() {
        if (!rowData) return i18n("Unknown");
        if (rowData.availability === "disabled") return i18n("Unavailable");
        if (rowData.precision === "availability_only") return i18n("Available");
        var unit = rowData.unit || "";
        if (rowData.used !== undefined && rowData.limit !== undefined && rowData.limit > 0) {
            return rowData.used + " / " + rowData.limit;
        }
        if (rowData.remaining !== undefined && rowData.limit !== undefined && rowData.limit > 0) {
            return i18n("%1 left", rowData.remaining);
        }
        if (rowData.percentRemaining !== undefined) {
            return Math.round(rowData.percentRemaining) + "%";
        }
        if (rowData.percentUsed !== undefined) {
            return Math.round(rowData.percentUsed) + "%";
        }
        if (rowData.rangeMin !== undefined && rowData.rangeMax !== undefined) {
            return rowData.rangeMin + "-" + rowData.rangeMax;
        }
        if (rowData.limit !== undefined) {
            if (unit === "usage_multiplier") return rowData.limit + "x";
            return rowData.limit + "";
        }
        return i18n("See note");
    }
}
