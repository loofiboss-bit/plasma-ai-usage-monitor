import QtQuick
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0
import "components" as Components

KCM.SimpleKCM {
    id: historyPage

    property alias cfg_historyEnabled: historySwitch.checked
    property alias cfg_historyRetentionDays: retentionSlider.value
    property alias cfg_prometheusEnabled: prometheusSwitch.checked
    property alias cfg_prometheusPort: prometheusPortSpin.value
    property alias cfg_prometheusListenAllInterfaces: prometheusNetworkSwitch.checked
    property alias cfg_autoExportEnabled: autoExportSwitch.checked
    property alias cfg_autoExportDirectory: autoExportDirectoryField.text
    property alias cfg_autoExportIntervalMinutes: autoExportIntervalSpin.value
    property string cfg_autoExportFormat: Plasmoid.configuration.autoExportFormat

    function readRuntimeSnapshot(value) {
        try {
            var snapshot = JSON.parse(value || "{}");
            var updated = Date.parse(snapshot.timestamp || "");
            if (!snapshot.sessionId || !isFinite(updated) || Date.now() - updated > 60000
                    || updated > Date.now() + 5000) return null;
            return snapshot;
        } catch (error) {
            return null;
        }
    }

    function prometheusRuntimeText() {
        var snapshot = readRuntimeSnapshot(Plasmoid.configuration.prometheusRuntimeSnapshot);
        if (!snapshot) return i18n("Runtime status unknown. No recent status snapshot is available.");
        if (snapshot.status === "disabled") return i18n("Runtime status: disabled.");
        if (snapshot.status === "starting") return i18n("Runtime status: starting the metrics endpoint.");
        if (snapshot.status === "error") {
            var detail = snapshot.errorCode === "address-in-use" ? i18n("the port is already in use")
                : snapshot.errorCode === "address-unavailable" ? i18n("the listen address is unavailable")
                : snapshot.errorCode === "permission-denied" ? i18n("permission was denied")
                : snapshot.errorCode === "unsupported-operation" ? i18n("the listen operation is unsupported")
                : i18n("the endpoint could not start");
            return i18n("Runtime status: failed to listen on port %1 because %2.", snapshot.port, detail);
        }
        if (snapshot.status === "listening") {
            var access = snapshot.interfaceMode === "all-interfaces"
                ? i18n("all IPv4 interfaces") : i18n("local interface only");
            return i18n("Runtime status: listening at %1:%2 (%3).", snapshot.address, snapshot.port, access);
        }
        return i18n("Runtime status unknown. The last status value was not recognized.");
    }

    // Database reference for size display
    UsageDatabase {
        id: historyDb
        enabled: Plasmoid.configuration.historyEnabled
        retentionDays: Plasmoid.configuration.historyRetentionDays
    }

    Components.HistoryMaintenanceController {
        id: maintenance
        database: historyDb
    }
    Timer {
        interval: 30000
        running: historyPage.visible && !maintenance.busy
        repeat: true
        onTriggered: maintenance.refreshStorage()
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        // Master toggle
        QQC2.Switch {
            id: historySwitch
            Kirigami.FormData.label: i18n("Enable history:")
            checked: Plasmoid.configuration.historyEnabled
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
                value: Plasmoid.configuration.historyRetentionDays
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

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: maintenance.storage.status === "failed"
            type: Kirigami.MessageType.Error
            text: i18n("History storage could not be opened or inspected. Error: %1", maintenance.storage.errorKey || "")
            Accessible.name: text
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: maintenance.busy || !!maintenance.lastResult.status
            type: maintenance.lastResult.status === "failed" ? Kirigami.MessageType.Error
                : maintenance.lastResult.status === "partial" ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
            text: maintenance.busy ? i18n("History operation in progress…")
                : historyPage.operationText(maintenance.lastResult)
            Accessible.name: text
        }

        // Database size
        QQC2.Label {
            Kirigami.FormData.label: i18n("Database size:")
            text: maintenance.storageAvailable
                ? i18n("%1 database + %2 WAL", historyPage.formatBytes(maintenance.storage.databaseBytes), historyPage.formatBytes(maintenance.storage.walBytes))
                : i18n("Unavailable")
        }

        // Providers with data
        QQC2.Label {
            Kirigami.FormData.label: i18n("Providers tracked:")
            text: {
                if (!maintenance.storageAvailable) return i18n("Unavailable");
                var providers = maintenance.storage.sources || [];
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
            enabled: maintenance.storageAvailable && !maintenance.busy
            onClicked: maintenance.prune()
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Last scheduled export:")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: maintenance.storage.lastScheduledExport
                ? historyPage.operationText(maintenance.storage.lastScheduledExport)
                : i18n("No completed scheduled export")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Prometheus")
        }

        QQC2.Switch {
            id: prometheusSwitch
            Kirigami.FormData.label: i18n("Enable metrics endpoint:")
            checked: Plasmoid.configuration.prometheusEnabled
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Runtime status:")
            text: historyPage.prometheusRuntimeText()
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Accessible.name: text
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Port:")
            enabled: prometheusSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.SpinBox {
                id: prometheusPortSpin
                from: 1024
                to: 65535
                value: Plasmoid.configuration.prometheusPort
            }

            QQC2.Label {
                text: prometheusNetworkSwitch.checked
                    ? i18n("Served on all IPv4 interfaces")
                    : i18n("Served locally on 127.0.0.1 only")
                color: Kirigami.Theme.disabledTextColor
                Layout.fillWidth: true
            }
        }

        QQC2.Switch {
            id: prometheusNetworkSwitch
            Kirigami.FormData.label: i18n("Network access:")
            text: i18n("Listen on all IPv4 interfaces")
            enabled: prometheusSwitch.checked
            checked: Plasmoid.configuration.prometheusListenAllInterfaces
        }

        Kirigami.InlineMessage {
            visible: prometheusSwitch.checked && prometheusNetworkSwitch.checked
            type: Kirigami.MessageType.Warning
            text: i18n("The metrics endpoint has no authentication or encryption. Anyone who can reach this port can read the exported metrics. Restrict access with a firewall.")
            Accessible.name: text
            Layout.fillWidth: true
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Auto Export")
        }

        QQC2.Switch {
            id: autoExportSwitch
            Kirigami.FormData.label: i18n("Enable scheduled export:")
            checked: Plasmoid.configuration.autoExportEnabled
        }

        QQC2.TextField {
            id: autoExportDirectoryField
            Kirigami.FormData.label: i18n("Directory:")
            enabled: autoExportSwitch.checked
            text: Plasmoid.configuration.autoExportDirectory
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
                value: Plasmoid.configuration.autoExportIntervalMinutes
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
            enabled: autoExportDirectoryField.text.trim().length > 0 && !maintenance.busy
            text: i18n("Write Export Files")
            onClicked: {
                var formats = ["json", "csv"];
                if (historyPage.cfg_autoExportFormat === "json") {
                    formats = ["json"];
                } else if (historyPage.cfg_autoExportFormat === "csv") {
                    formats = ["csv"];
                }
                maintenance.exportFiles(autoExportDirectoryField.text, formats);
            }
        }
    }

    function operationText(result) {
        if (result.status === "failed")
            return i18n("Operation failed. Error: %1", result.errorKey || "");
        if (result.rowsDeleted !== undefined)
            return i18n("Removed %1 expired rows. Storage statistics are up to date.", result.rowsDeleted);
        var paths = result.paths || [];
        var summary = result.status === "partial"
            ? i18n("Export partially completed: %1 of %2 files.", paths.length, result.expectedFiles || 0)
            : i18n("Export completed: %1 files.", paths.length);
        return summary + "\n" + paths.join("\n")
            + (result.completedAtUtc ? "\n" + result.completedAtUtc : "");
    }

    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }
}
