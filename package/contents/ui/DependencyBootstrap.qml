import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

PlasmaExtras.Representation {
    id: bootstrap

    required property string frontendVersion
    required property string installedPluginVersion
    required property string bootstrapState
    required property string supportReport

    readonly property string installCommand: "sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor && sudo dnf install --refresh plasma-ai-usage-monitor"
    readonly property url sourceInstallUrl: "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/installation.md#guided-source-install"
    readonly property bool checking: bootstrapState === "idle"
                                     || bootstrapState === "loading-probe"
                                     || bootstrapState === "loading-runtime"
    readonly property bool mismatch: bootstrapState === "plugin-older"
                                     || bootstrapState === "plugin-newer"

    implicitWidth: Kirigami.Units.gridUnit * 28
    implicitHeight: Kirigami.Units.gridUnit * 28

    function titleForState() {
        if (bootstrapState === "plugin-older")
            return KI18n.i18n("The native plugin is older than the widget");
        if (bootstrapState === "plugin-newer")
            return KI18n.i18n("The native plugin is newer than the widget");
        if (bootstrapState === "runtime-unavailable")
            return KI18n.i18n("The native plugin could not start");
        if (checking)
            return KI18n.i18n("Checking the native plugin…");
        return KI18n.i18n("Install the native plugin");
    }

    function descriptionForState() {
        if (mismatch)
            return KI18n.i18n("Update the KDE Store widget and the Fedora package so both use the same version.");
        if (bootstrapState === "runtime-unavailable")
            return KI18n.i18n("The plugin was found, but the monitor could not load. Reinstall the matching package.");
        if (checking)
            return KI18n.i18n("AI Usage Monitor is checking whether its compiled dependency is ready.");
        return KI18n.i18n("The KDE Store package contains the widget frontend only. Install the matching compiled plugin from Fedora COPR or build it from source.");
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        Item { Layout.fillHeight: true }

        Kirigami.Icon {
            source: bootstrap.checking ? "view-refresh" : "dialog-warning"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: width
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            level: 2
            text: bootstrap.titleForState()
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: bootstrap.descriptionForState()
        }

        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                text: KI18n.i18n("Widget frontend:")
                opacity: 0.7
            }
            PlasmaComponents.Label { text: bootstrap.frontendVersion }

            PlasmaComponents.Label {
                text: KI18n.i18n("Native plugin:")
                opacity: 0.7
            }
            PlasmaComponents.Label {
                text: bootstrap.installedPluginVersion !== ""
                      ? bootstrap.installedPluginVersion
                      : KI18n.i18n("Not detected")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: !bootstrap.checking
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Fedora COPR command")
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.TextField {
                    id: commandField
                    Layout.fillWidth: true
                    readOnly: true
                    selectByMouse: true
                    text: bootstrap.installCommand
                    Accessible.name: KI18n.i18n("Fedora COPR install command")
                }

                PlasmaComponents.Button {
                    text: copiedTimer.running ? KI18n.i18n("Copied") : KI18n.i18n("Copy")
                    icon.name: "edit-copy"
                    onClicked: {
                        commandField.selectAll();
                        commandField.copy();
                        commandField.deselect();
                        copiedTimer.restart();
                    }
                }
            }

            PlasmaComponents.Button {
                Layout.alignment: Qt.AlignHCenter
                text: KI18n.i18n("Open source installation guide")
                icon.name: "help-contents"
                onClicked: Qt.openUrlExternally(bootstrap.sourceInstallUrl)
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Support report")
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.TextArea {
                    id: supportReportField
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 4
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    text: bootstrap.supportReport
                    Accessible.name: KI18n.i18n("Bootstrap support report")
                }

                PlasmaComponents.Button {
                    text: reportCopiedTimer.running ? KI18n.i18n("Copied") : KI18n.i18n("Copy report")
                    icon.name: "edit-copy"
                    onClicked: {
                        supportReportField.selectAll();
                        supportReportField.copy();
                        supportReportField.deselect();
                        reportCopiedTimer.restart();
                    }
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !bootstrap.checking
            type: Kirigami.MessageType.Information
            text: KI18n.i18n("After installing or updating, restart Plasma or log out and back in. Your settings, KWallet secrets, and history are kept.")
        }

        Item { Layout.fillHeight: true }
    }

    Timer {
        id: copiedTimer
        interval: 2000
    }

    Timer {
        id: reportCopiedTimer
        interval: 2000
    }
}
