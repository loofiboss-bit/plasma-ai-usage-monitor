pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: header

    required property var summary
    readonly property bool narrow: width < Kirigami.Units.gridUnit * 22

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.06)
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.2)
    Accessible.role: Accessible.StaticText
    Accessible.name: headline() + ". " + accessibleSummary()

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        PlasmaExtras.Heading {
            level: 3
            Layout.fillWidth: true
            text: header.headline()
            wrapMode: Text.WordWrap
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: header.explanation()
            wrapMode: Text.WordWrap
            color: header.summary.attention > 0
                ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
        }

        GridLayout {
            Layout.fillWidth: true
            columns: header.narrow ? 2 : 5
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            Repeater {
                model: [
                    { count: header.summary.actual, label: i18n("Actual data"), icon: "dialog-ok" },
                    { count: header.summary.estimate, label: i18n("Estimates / local"), icon: "view-statistics" },
                    { count: header.summary.balance, label: i18n("Balance"), icon: "wallet-open" },
                    { count: header.summary.connectivity, label: i18n("Connectivity only"), icon: "network-connect" },
                    { count: header.summary.attention, label: i18n("Needs attention"), icon: "dialog-warning" }
                ]

                Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: statRow.implicitHeight + Kirigami.Units.smallSpacing * 2
                    radius: Kirigami.Units.smallSpacing
                    color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.65)
                    border.width: 1
                    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.12)

                    RowLayout {
                        id: statRow
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: modelData.icon
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: width
                        }
                        PlasmaComponents.Label {
                            text: modelData.count
                            font.bold: true
                        }
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            elide: Text.ElideRight
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }
                    }
                }
            }
        }
    }

    function headline() {
        if (summary.useful > 0)
            return i18np("%1 source is reporting useful data", "%1 sources are reporting useful data", summary.useful);
        if (summary.connectivity > 0)
            return i18n("Connected sources are not reporting usage or spend");
        if (summary.attention > 0)
            return i18n("Sources need attention before data can report");
        if (summary.verifying > 0)
            return i18n("Checking source data…");
        return i18n("No monitoring source is enabled");
    }

    function explanation() {
        if (summary.useful > 0 && summary.attention > 0)
            return i18np("Useful data is available, but %1 source still needs action.",
                         "Useful data is available, but %1 sources still need action.", summary.attention);
        if (summary.useful > 0)
            return i18n("Values below keep actual data, estimates, balances, and currencies separate.");
        if (summary.connectivity > 0)
            return i18n("A successful connection check does not prove token usage, spend, or quota data.");
        if (summary.attention > 0)
            return i18n("Use the Fix actions below to restore or verify each source.");
        return i18n("Run guided setup to choose and verify a useful source.");
    }

    function accessibleSummary() {
        return i18n("Actual data: %1. Estimates or local activity: %2. Balances: %3. Connectivity only: %4. Needs attention: %5.",
                    summary.actual, summary.estimate, summary.balance,
                    summary.connectivity, summary.attention);
    }
}
