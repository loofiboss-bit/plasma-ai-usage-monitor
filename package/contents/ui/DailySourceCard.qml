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
    signal actionRequested(string stableId, string actionKey, string sourceKind)
    signal sourceRequested(string stableId)

    objectName: "sourceCard-" + (row.stableId || "")
    width: parent ? parent.width : implicitWidth
    activeFocusOnTab: true
    Accessible.name: row.displayName + ". " + attentionText() + stateText()
        + ". " + metricText() + ". " + i18n("Open source details")
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
                text: card.metricStatusText()
                color: card.row.freshnessState === "stale"
                    ? Kirigami.Theme.neutralTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                Layout.alignment: Qt.AlignRight
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
        if (row.primaryMetricAvailable !== true) return "\u2014";
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
            return i18np("%1 token", "%1 tokens", Math.round(value));
        if (row.primaryMetricUnit === "request")
            return i18np("%1 request", "%1 requests", Math.round(value));
        if (row.primaryMetricUnit === "credits")
            return i18np("%1 credit", "%1 credits", Math.round(value));
        if (row.primaryMetricUnit === "count")
            return i18np("%1 event", "%1 events", Math.round(value));
        return value.toLocaleString(Qt.locale());
    }

    function metricStatusText() {
        var status = row.freshnessState === "stale" ? i18n("Stale") : i18n("Current");
        var period = Utils.spendWindowText({
            window: row.primaryMetricWindow || "",
            periodStart: row.primaryMetricPeriodStart,
            periodEnd: row.primaryMetricPeriodEnd,
            bounded: !!row.primaryMetricPeriodStart && !!row.primaryMetricPeriodEnd,
            sourceNames: [row.displayName || row.stableId]
        });
        status += i18n(" · %1", period);
        return status;
    }
}
