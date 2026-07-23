pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

Rectangle {
    id: card

    required property var row
    signal actionRequested(string stableId, string actionKey, string sourceKind)

    implicitHeight: content.implicitHeight + Kirigami.Units.mediumSpacing * 2
    radius: Kirigami.Units.smallSpacing
    color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.72)
    border.width: 1
    border.color: Qt.alpha(accentColor(), 0.24)
    Accessible.role: Accessible.StaticText
    Accessible.name: row.displayName + ". " + attentionText() + stateText()
        + ". " + metricText()

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.mediumSpacing
        spacing: Kirigami.Units.mediumSpacing

        Kirigami.Icon {
            source: card.row.sourceKind === "local_tool" ? "utilities-terminal" : "globe"
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredHeight: width
            color: card.accentColor()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: card.row.displayName || card.row.stableId
                font.bold: true
                elide: Text.ElideRight
            }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: card.attentionText() + card.stateText()
                color: card.row.attentionSeverity === "critical"
                    ? Kirigami.Theme.negativeTextColor
                    : card.row.attentionSeverity === "warning"
                        ? Kirigami.Theme.neutralTextColor
                        : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            visible: card.row.primaryMetricAvailable === true
            spacing: 0
            PlasmaComponents.Label {
                objectName: "sourceMetricValue"
                text: card.metricText()
                font.bold: true
                horizontalAlignment: Text.AlignRight
                Layout.alignment: Qt.AlignRight
            }
            PlasmaComponents.Label {
                text: card.row.freshnessState === "stale" ? i18n("Stale") : i18n("Current")
                color: card.row.freshnessState === "stale"
                    ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                Layout.alignment: Qt.AlignRight
            }
        }

        PlasmaComponents.ToolButton {
            visible: card.row.attentionSeverity && card.row.attentionSeverity !== "none"
            icon.name: card.row.nextActionKey === "refresh_stale_data" ? "view-refresh" : "configure"
            activeFocusOnTab: true
            Accessible.name: i18n("Review %1", card.row.displayName)
            onClicked: card.actionRequested(card.row.stableId, card.row.nextActionKey,
                                            card.row.sourceKind)
        }
    }

    function accentColor() {
        if (row.attentionSeverity === "critical") return Kirigami.Theme.negativeTextColor;
        if (row.attentionSeverity === "warning") return Kirigami.Theme.neutralTextColor;
        if (row.qualityClass === "connectivity_only") return Kirigami.Theme.disabledTextColor;
        return Kirigami.Theme.highlightColor;
    }

    function stateText() {
        var labels = {
            actual: i18n("Provider-reported data"),
            estimated: i18n("Local estimate"),
            balance: i18n("Account balance"),
            connectivity_only: i18n("Connectivity only"),
            unavailable: i18n("Data unavailable")
        };
        return labels[row.qualityClass] || i18n("Data unavailable");
    }

    function attentionText() {
        if (row.attentionSeverity === "critical") return i18n("Critical · ");
        if (row.attentionSeverity === "warning") return i18n("Warning · ");
        return "";
    }

    function metricText() {
        if (row.primaryMetricAvailable !== true) return "\u2014";
        var value = Number(row.primaryMetricValue);
        if (!Number.isFinite(value)) return "\u2014";
        if (row.primaryMetricUnit === "percent_remaining")
            return i18n("%1% left", Math.round(value));
        if (row.currency && row.currency !== "MIXED")
            return i18n("%1 %2", row.currency, value.toLocaleString(Qt.locale(), "f", 2));
        return value.toLocaleString(Qt.locale());
    }
}
