pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.Card {
    id: card

    required property var forecast
    property bool showHistoryAction: false
    signal historyRequested(string metric)

    Accessible.role: Accessible.Grouping
    Accessible.name: stateLabel() + ". " + summaryText()

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            source: card.stateIcon()
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: width
        }
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 4
            text: card.forecast.kind === "budget_overrun"
                ? i18n("Monthly budget pacing")
                : i18n("Quota runway")
            elide: Text.ElideRight
        }
        QQC2.Label {
            objectName: "runwayStateLabel"
            text: card.stateLabel()
            font.bold: true
            color: card.forecast.state === "critical"
                ? Kirigami.Theme.negativeTextColor
                : card.forecast.state === "warning"
                  ? Kirigami.Theme.neutralTextColor
                  : card.forecast.state === "safe"
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
        }
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            objectName: "runwaySummary"
            Layout.fillWidth: true
            text: card.summaryText()
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: card.width < Kirigami.Units.gridUnit * 17 ? 1 : 3
            rowSpacing: Kirigami.Units.smallSpacing
            columnSpacing: Kirigami.Units.largeSpacing
            visible: card.forecast.state !== "unavailable"

            Repeater {
                model: [
                    {
                        label: i18n("Current"),
                        value: card.forecast.currentValue
                    },
                    {
                        label: i18n("Projected"),
                        value: card.forecast.projectedValue
                    },
                    {
                        label: i18n("Limit"),
                        value: card.forecast.limitValue
                    }
                ]

                ColumnLayout {
                    id: valueColumn
                    required property var modelData
                    spacing: 0
                    Layout.fillWidth: true

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: valueColumn.modelData.label
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: card.formatValue(valueColumn.modelData.value)
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Period ends %1", card.formatDate(
                           card.forecast.periodEnd))
            visible: card.forecast.periodEnd !== undefined
                && card.forecast.periodEnd !== null
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Evidence: %1 · %2 samples · %3% coverage",
                       card.evidenceLabel(),
                       Number(card.forecast.sampleCount || 0),
                       Number(card.forecast.coveragePercent || 0)
                           .toLocaleString(Qt.locale(), "f", 0))
            color: Kirigami.Theme.disabledTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Method: %1", card.forecast.methodId || "—")
            color: Kirigami.Theme.disabledTextColor
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WrapAnywhere
            Accessible.name: i18n("Deterministic calculation method: %1",
                                  card.forecast.methodId || i18n("Unavailable"))
        }

        QQC2.Button {
            visible: card.showHistoryAction
            text: i18n("Open supporting history")
            icon.name: "view-history"
            activeFocusOnTab: true
            Accessible.name: text
            onClicked: card.historyRequested(
                card.forecast.kind === "budget_overrun"
                    ? "cost" : card.quotaMetric())
        }
    }

    function stateIcon() {
        switch (forecast.state) {
        case "critical": return "dialog-error-symbolic";
        case "warning": return "dialog-warning-symbolic";
        case "safe": return "dialog-ok-symbolic";
        default: return "question";
        }
    }

    function stateLabel() {
        switch (forecast.state) {
        case "critical": return i18n("Critical");
        case "warning": return i18n("Warning");
        case "safe": return i18n("Safe");
        default: return i18n("Unavailable");
        }
    }

    function evidenceLabel() {
        switch (forecast.evidenceGrade) {
        case "strong": return i18n("Strong");
        case "usable": return i18n("Usable");
        default: return i18n("Unavailable");
        }
    }

    function summaryText() {
        if (forecast.state === "unavailable")
            return forecast.reasonText || i18n("A compatible forecast is not available.");
        if (forecast.predictedAt !== undefined
                && forecast.predictedAt !== null) {
            return forecast.kind === "budget_overrun"
                ? i18n("The configured budget is projected to be exceeded on %1.",
                       formatDate(forecast.predictedAt))
                : i18n("Quota is projected to be exhausted on %1, before reset.",
                       formatDate(forecast.predictedAt));
        }
        return forecast.kind === "budget_overrun"
            ? i18n("Projected month-end spend stays within the configured budget.")
            : i18n("No exhaustion is projected before this quota resets.");
    }

    function formatValue(value) {
        if (value === undefined || value === null) return "—";
        var number = Number(value);
        if (!Number.isFinite(number)) return "—";
        var formatted = number.toLocaleString(
            Qt.locale(), "f", forecast.currency ? 2 : 0);
        return forecast.currency
            ? i18n("%1 %2", forecast.currency, formatted)
            : i18n("%1 %2", formatted, forecast.unit || "");
    }

    function formatDate(value) {
        if (value === undefined || value === null) return "—";
        var date = new Date(value);
        return isNaN(date.getTime()) ? "—"
                                    : date.toLocaleString(Qt.locale());
    }

    function quotaMetric() {
        return String(forecast.window || "").indexOf("token") >= 0
            ? "tokens" : "requests";
    }
}
