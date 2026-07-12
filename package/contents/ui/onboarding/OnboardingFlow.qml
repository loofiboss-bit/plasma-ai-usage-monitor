import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Item {
    id: flow
    property int step: 0

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 22)
        height: content.implicitHeight + Kirigami.Units.largeSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.2)

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.mediumSpacing

            PlasmaExtras.Heading { level: 3; text: i18n("Set up AI Usage Monitor") }
            PlasmaComponents.Label {
                text: i18n("Step %1 of 4", flow.step + 1)
                opacity: 0.7
            }
            PlasmaExtras.Heading {
                level: 4
                text: [i18n("Choose data sources"), i18n("Configure credentials and plans"),
                       i18n("Test selected connections"), i18n("Review data quality")][flow.step]
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: [
                    i18n("Enable API providers, local subscription tools, or both."),
                    i18n("API keys stay in KWallet. Browser Sync is optional, local, and disabled by default."),
                    i18n("Use Diagnostics to verify connectivity without treating a probe as billing data."),
                    i18n("The widget labels actual, estimated, probe-only, stale, and mixed-currency data separately.")
                ][flow.step]
            }
            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents.Button {
                    text: i18n("Back")
                    enabled: flow.step > 0
                    onClicked: flow.step--
                }
                Item { Layout.fillWidth: true }
                PlasmaComponents.Button {
                    text: flow.step < 3 ? i18n("Next") : i18n("Open Settings")
                    onClicked: {
                        if (flow.step < 3) flow.step++;
                        else plasmoid.internalAction("configure").trigger();
                    }
                }
            }
        }
    }
}
