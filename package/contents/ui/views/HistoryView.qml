pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import ".." as Monitor
import "../components" as Components

ColumnLayout {
    id: history

    property string pendingExportFormat: "csv"
    property string exportStatus: ""

    Components.HistoryController {
        id: historyController
        usageDb: root.usageDb
        dailyState: root.dailyState
        configuredProviders: root.allProviders || []
        configuredTools: root.allSubscriptionTools || []
    }

    function openExport(format) {
        pendingExportFormat = format;
        exportDialog.nameFilters = format === "json"
            ? [i18n("JSON files (*.json)")] : [i18n("CSV files (*.csv)")];
        exportDialog.currentFile = "ai-usage-history." + format;
        exportDialog.open();
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            level: 4
            text: i18n("History")
            Layout.fillWidth: true
        }
        PlasmaComponents.ToolButton {
            text: historyController.compareMode ? i18n("Compare") : i18n("Detail")
            checkable: true
            checked: historyController.compareMode
            onClicked: {
                historyController.compareMode = !historyController.compareMode;
                historyController.normalizeSelection();
                Qt.callLater(historyController.refresh);
            }
        }
        PlasmaComponents.ToolButton {
            icon.name: "view-refresh"
            enabled: !historyController.loading
            Accessible.name: i18n("Refresh history")
            onClicked: historyController.refreshCatalog()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        QQC2.ComboBox {
            id: rangeCombo
            model: [i18n("24 hours"), i18n("7 days"), i18n("30 days"), i18n("90 days")]
            currentIndex: historyController.rangeIndex
            Accessible.name: i18n("History range")
            onActivated: {
                historyController.rangeIndex = currentIndex;
                historyController.refresh();
            }
        }
        QQC2.ComboBox {
            id: sourceCombo
            visible: !historyController.compareMode
            Layout.fillWidth: true
            model: historyController.sourceOptions()
            textRole: "text"
            valueRole: "value"
            currentIndex: historyController.sourceIndex()
            Accessible.name: i18n("History source")
            onActivated: {
                historyController.selectedSourceId = currentValue || "";
                historyController.normalizeSelection();
                Qt.callLater(historyController.refresh);
            }
        }
        QQC2.ComboBox {
            id: metricCombo
            Layout.fillWidth: historyController.compareMode
            model: historyController.metricRows()
            textRole: "text"
            valueRole: "value"
            currentIndex: historyController.metricIndex()
            Accessible.name: i18n("History metric")
            onActivated: {
                historyController.selectedMetric = currentValue || "";
                historyController.refresh();
            }
        }
        PlasmaComponents.Button {
            text: i18n("Export file")
            icon.name: "document-export"
            enabled: !historyController.loading
                && historyController.seriesData.length > 0
            onClicked: exportMenu.open()

            QQC2.Menu {
                id: exportMenu
                y: parent.height
                QQC2.MenuItem {
                    text: i18n("Export CSV file")
                    onTriggered: history.openExport("csv")
                }
                QQC2.MenuItem {
                    text: i18n("Export JSON file")
                    onTriggered: history.openExport("json")
                }
            }
        }
        PlasmaComponents.ToolButton {
            icon.name: "edit-copy"
            enabled: !historyController.loading
                && historyController.seriesData.length > 0
            Accessible.name: i18n("Copy history as CSV")
            onClicked: clipboard.setText(historyController.exportPayload("csv"))
        }
    }

    PlasmaComponents.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        visible: historyController.loading
        running: visible
    }

    PlasmaComponents.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        visible: history.exportStatus !== ""
        text: history.exportStatus
        color: Kirigami.Theme.neutralTextColor
        wrapMode: Text.WordWrap
        Accessible.name: text
    }

    Components.EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !historyController.loading
            && (historyController.errorKey !== ""
                || historyController.seriesData.length === 0)
        title: historyController.errorKey !== ""
            ? i18n("This comparison is not compatible")
            : i18n("No compatible history data")
        details: historyController.errorKey !== ""
            ? historyController.errorText()
            : i18n("Choose a source, metric, and time range with retained compatible observations.")
    }

    Monitor.MultiSeriesChart {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !historyController.loading && historyController.errorKey === ""
            && historyController.seriesData.length > 0
        seriesData: historyController.seriesData
        metric: historyController.activeMetric || "cost"
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        visible: !historyController.loading
            && historyController.seriesData.length > 0
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: historyController.seriesData
            PlasmaComponents.Label {
                required property var modelData
                Layout.fillWidth: true
                text: i18nc("History coverage metadata", "%1: %2",
                            modelData.name,
                            historyController.coverageText(modelData))
                color: Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                Accessible.name: text
            }
        }
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export History")
        fileMode: Dialogs.FileDialog.SaveFile
        onAccepted: {
            history.exportStatus = AppInfo.exportConfig(
                historyController.exportPayload(history.pendingExportFormat),
                selectedFile.toString())
                ? i18n("History export saved.")
                : i18n("The history export could not be written.");
        }
    }

    ClipboardHelper {
        id: clipboard
    }

    Component.onCompleted: historyController.refreshCatalog()
}
