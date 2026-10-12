pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

QQC2.ItemDelegate {
    id: card

    required property var row
    property string sessionActivityText: ""
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    signal sourceRequested(string stableId)

    objectName: "sourceCard-" + (row.stableId || "")
    width: parent ? parent.width : implicitWidth
    activeFocusOnTab: true
    Accessible.name: row.displayName + ". " + attentionText() + stateText()
        + ". " + metricText() + ". " + sessionActivityText
        + ". " + i18n("Open source details")
    Accessible.description: exactMetricText()
    QQC2.ToolTip.visible: hovered || activeFocus
    QQC2.ToolTip.text: exactMetricText()
    onClicked: sourceRequested(row.stableId)

    background: Rectangle {
        radius: Kirigami.Units.smallSpacing
        color: card.hovered || card.activeFocus
            ? Qt.alpha(Kirigami.Theme.highlightColor, 0.1)
            : Qt.alpha(Kirigami.Theme.backgroundColor, 0.72)
        border.width: 1
        border.color: Qt.alpha(card.accentColor(), card.activeFocus ? 0.65 : 0.24)
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.mediumSpacing

        Kirigami.Icon {
            objectName: "sourceLogo"
            source: card.logoSource()
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: width
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

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
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: card.sessionActivityText !== ""
                    text: card.sessionActivityText
                    color: Kirigami.Theme.disabledTextColor
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    elide: Text.ElideRight
                }
            }

            ColumnLayout {
                Layout.minimumWidth: 0
                visible: card.row.primaryMetricAvailable === true
                spacing: 0
                Layout.alignment: Qt.AlignRight

                PlasmaComponents.Label {
                    objectName: "sourceMetricValue"
                    text: card.metricText()
                    font.bold: true
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight
                }
                PlasmaComponents.Label {
                    wrapMode: Text.NoWrap
                    text: card.metricStatusText()
                    color: card.row.freshnessState === "stale"
                        ? Kirigami.Theme.neutralTextColor
                        : Kirigami.Theme.disabledTextColor
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight
                }
            }
        }

        Kirigami.Icon {
            source: "go-next-symbolic"
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: width
        }
    }

    function accentColor() {
        if (row.attentionSeverity === "critical") return Kirigami.Theme.negativeTextColor;
        if (row.attentionSeverity === "warning") return Kirigami.Theme.neutralTextColor;
        if (row.qualityClass === "connectivity_only") return Kirigami.Theme.disabledTextColor;
        return Kirigami.Theme.highlightColor;
    }

    function logoSource() {
        if (row.iconSource) return row.iconSource;
        if (row.sourceKind !== "local_tool")
            return Qt.resolvedUrl("../icons/providers/" + row.stableId + ".svg");
        var toolIcons = {
            "google-antigravity": "antigravity",
            "claude-code": "claude-code",
            "codex-cli": "codex-cli",
            "github-copilot": "copilot",
            cursor: "cursor",
            windsurf: "windsurf",
            "jetbrains-ai": "jetbrains"
        };
        var iconName = toolIcons[row.stableId];
        return iconName
            ? Qt.resolvedUrl("../icons/tools/" + iconName + ".svg")
            : "utilities-terminal";
    }

    function stateText() {
        var quality = row.primaryMetricAvailable === true
            ? (row.primaryMetricQuality || row.qualityClass) : row.qualityClass;
        var labels = {
            actual: i18n("Provider-reported data"),
            estimated: i18n("Local estimate"),
            balance: i18n("Account balance"),
            configured_limit: i18n("Configured limit"),
            connectivity_only: i18n("Connectivity only"),
            unavailable: i18n("Data unavailable")
        };
        return labels[quality] || i18n("Data unavailable");
    }

    function attentionText() {
        if (row.attentionSeverity === "critical") return i18n("Critical · ");
        if (row.attentionSeverity === "warning") return i18n("Warning · ");
        return "";
    }

    function metricText() {
        if (row.primaryMetricAvailable !== true
                || row.primaryMetricValue === undefined
                || row.primaryMetricValue === null
                || row.primaryMetricValue === "") return "\u2014";
        var value = Number(row.primaryMetricValue);
        if (!Number.isFinite(value)) return "\u2014";
        if (row.primaryMetricUnit === "percent_remaining")
            return i18n("%1% left", Math.round(value));
        if (row.primaryMetricKind === "cost"
                || row.primaryMetricKind === "credit_balance"
                && !!row.primaryMetricCurrency) {
            var currency = row.primaryMetricCurrency || "";
            return currency !== ""
                ? i18n("%1 %2", currency,
                       value.toLocaleString(Qt.locale(), "f", 2))
                : value.toLocaleString(Qt.locale(), "f", 2);
        }
        if (row.primaryMetricUnit === "token")
            return i18n("%1 tokens", Utils.formatNumber(value));
        if (row.primaryMetricUnit === "request")
            return i18n("%1 requests", value.toLocaleString(Qt.locale(), "f", 0));
        if (row.primaryMetricUnit === "credits")
            return i18np("%1 credit", "%1 credits", Math.round(value));
        if (row.primaryMetricUnit === "count")
            return i18n("%1 events", value.toLocaleString(Qt.locale(), "f", 0));
        return value.toLocaleString(Qt.locale());
    }

    function exactMetricText() {
        if (row.primaryMetricAvailable !== true
                || row.primaryMetricValue === null
                || row.primaryMetricValue === undefined
                || row.primaryMetricValue === "")
            return i18n("Unavailable");
        var value = Number(row.primaryMetricValue);
        if (!Number.isFinite(value)) return i18n("Unavailable");
        var formatted = row.primaryMetricUnit === "token"
            || row.primaryMetricUnit === "request"
            || row.primaryMetricUnit === "count"
            ? value.toLocaleString(Qt.locale(), "f", 0)
            : value.toLocaleString(Qt.locale(), "f", 2);
        return i18n("%1 %2 · %3", formatted,
                    row.primaryMetricCurrency || row.primaryMetricUnit || "",
                    metricStatusText());
    }

    function metricStatusText() {
        var period = Utils.spendWindowText({
            window: row.primaryMetricWindow || "",
            periodStart: row.primaryMetricPeriodStart,
            periodEnd: row.primaryMetricPeriodEnd,
            bounded: !!row.primaryMetricPeriodStart && !!row.primaryMetricPeriodEnd,
            sourceNames: []
        });
        if (row.freshnessState === "stale") {
            return i18n("Stale · %1", period);
        }
        return period;
    }
}
