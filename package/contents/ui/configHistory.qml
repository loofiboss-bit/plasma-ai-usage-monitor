import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: historyPage

    property alias cfg_historyEnabled: historySwitch.checked
    property alias cfg_historyRetentionDays: retentionSlider.value
    property alias cfg_prometheusEnabled: prometheusSwitch.checked
    property alias cfg_prometheusPort: prometheusPortSpin.value
    property alias cfg_autoExportEnabled: autoExportSwitch.checked
    property alias cfg_autoExportDirectory: autoExportDirectoryField.text
    property alias cfg_autoExportIntervalMinutes: autoExportIntervalSpin.value
    property string cfg_autoExportFormat: plasmoid.configuration.autoExportFormat

    // Database reference for size display
    UsageDatabase {
        id: historyDb
        enabled: plasmoid.configuration.historyEnabled
        retentionDays: plasmoid.configuration.historyRetentionDays
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        // Master toggle
        QQC2.Switch {
            id: historySwitch
            Kirigami.FormData.label: i18n("Enable history:")
            checked: plasmoid.configuration.historyEnabled
        }

        QQC2.Label {
            text: i18n("When enabled, usage data is periodically saved to a local SQLite database for trend analysis and charts.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Data Retention")
        }

        // Retention period slider
        ColumnLayout {
            Kirigami.FormData.label: i18n("Keep data for:")
            enabled: historySwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: retentionSlider
                Layout.fillWidth: true
                from: 7
                to: 365
                stepSize: 1
                value: plasmoid.configuration.historyRetentionDays
            }

            QQC2.Label {
                text: {
                    var days = retentionSlider.value;
                    if (days >= 365) return i18n("1 year");
                    if (days >= 30) {
                        var months = Math.floor(days / 30);
                        var remainder = days % 30;
                        if (remainder > 0) {
                            return i18np("%1 month", "%1 months", months) + " " + i18np("%1 day", "%1 days", remainder);
                        }
                        return i18np("%1 month", "%1 months", months);
                    }
                    return i18np("%1 day", "%1 days", days);
                }
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
            }
        }

        QQC2.Label {
            enabled: historySwitch.checked
            text: i18n("Data older than this will be automatically pruned daily.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Storage")
        }

        // Database size
        QQC2.Label {
            Kirigami.FormData.label: i18n("Database size:")
            text: formatBytes(historyDb.databaseSize())
        }

        // Providers with data
        QQC2.Label {
            Kirigami.FormData.label: i18n("Providers tracked:")
            text: {
                var providers = historyDb.getProviders();
                return providers.length > 0 ? providers.join(", ") : i18n("None");
            }
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Prune now button
        QQC2.Button {
            Kirigami.FormData.label: i18n("Maintenance:")
            text: i18n("Prune Old Data Now")
            icon.name: "edit-clear-history"
            enabled: historySwitch.checked
            onClicked: {
                historyDb.pruneOldData();
                // Force refresh of displayed size
                dbSizeRefreshTimer.restart();
            }
        }

        // Invisible timer to refresh the db size after pruning
        Timer {
            id: dbSizeRefreshTimer
            interval: 500
            repeat: false
            // Trigger a binding re-evaluation by toggling a dummy property
            onTriggered: historyPage.forceActiveFocus()
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Prometheus")
        }

        QQC2.Switch {
            id: prometheusSwitch
            Kirigami.FormData.label: i18n("Enable metrics endpoint:")
            checked: plasmoid.configuration.prometheusEnabled
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Port:")
            enabled: prometheusSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.SpinBox {
                id: prometheusPortSpin
                from: 1024
                to: 65535
                value: plasmoid.configuration.prometheusPort
            }

            QQC2.Label {
                text: i18n("Served locally on 127.0.0.1 only")
                color: Kirigami.Theme.disabledTextColor
                Layout.fillWidth: true
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Auto Export")
        }

        QQC2.Switch {
            id: autoExportSwitch
            Kirigami.FormData.label: i18n("Enable scheduled export:")
            checked: plasmoid.configuration.autoExportEnabled
        }

        QQC2.TextField {
            id: autoExportDirectoryField
            Kirigami.FormData.label: i18n("Directory:")
            enabled: autoExportSwitch.checked
            text: plasmoid.configuration.autoExportDirectory
            placeholderText: i18n("/path/to/export-directory")
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Interval:")
            enabled: autoExportSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.SpinBox {
                id: autoExportIntervalSpin
                from: 5
                to: 1440
                value: plasmoid.configuration.autoExportIntervalMinutes
            }

            QQC2.Label {
                text: i18n("minutes")
                color: Kirigami.Theme.disabledTextColor
            }
        }

        QQC2.ComboBox {
            id: autoExportFormatCombo
            Kirigami.FormData.label: i18n("Format:")
            enabled: autoExportSwitch.checked
            model: [
                { label: i18n("JSON + CSV"), value: "both" },
                { label: i18n("JSON only"), value: "json" },
                { label: i18n("CSV only"), value: "csv" }
            ]
            textRole: "label"
            currentIndex: {
                if (historyPage.cfg_autoExportFormat === "json") return 1;
                if (historyPage.cfg_autoExportFormat === "csv") return 2;
                return 0;
            }
            onActivated: historyPage.cfg_autoExportFormat = model[currentIndex].value
        }

        QQC2.Button {
            Kirigami.FormData.label: i18n("Export now:")
            enabled: autoExportDirectoryField.text.length > 0
            text: i18n("Write Export Files")
            onClicked: {
                var formats = ["json", "csv"];
                if (historyPage.cfg_autoExportFormat === "json") {
                    formats = ["json"];
                } else if (historyPage.cfg_autoExportFormat === "csv") {
                    formats = ["csv"];
                }
                historyDb.requestExportAll("manual-" + Date.now(), autoExportDirectoryField.text, formats);
            }
        }
    }

    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }
}
