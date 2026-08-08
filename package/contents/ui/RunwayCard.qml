pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.Card {
    id: card

    required property var forecast
    property bool showHistoryAction: false
    readonly property bool budgetForecast: forecast.kind === "budget_overrun"
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
            text: card.budgetForecast
                ? i18n("Budget pacing")
                : i18n("Quota runway")
            elide: Text.ElideRight
        }
        QQC2.Label {
            objectName: "runwayStateLabel"
            text: card.stateLabel()
            font.bold: true
            color: card.forecast.state === "exceeded"
                || card.forecast.state === "critical"
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
                        label: card.budgetForecast ? i18n("Spent") : i18n("Current"),
                        value: card.forecast.currentValue,
                        minor: null
                    },
                    {
                        label: card.budgetForecast ? i18n("Remaining") : i18n("Projected"),
                        value: card.budgetForecast ? null
                            : card.forecast.projectedValue,
                        minor: card.budgetForecast
                            ? card.forecast.remainingMinor : null
                    },
                    {
                        label: card.budgetForecast ? i18n("Safe today") : i18n("Limit"),
                        value: null,
                        minor: card.budgetForecast
                            ? card.forecast.safeTodayMinor : null
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
                        text: valueColumn.modelData.minor !== null
                            && valueColumn.modelData.minor !== undefined
                            ? card.formatMinor(valueColumn.modelData.minor)
                            : card.formatValue(valueColumn.modelData.value)
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
            visible: card.budgetForecast
                && card.forecast.previousPeriodSpentMinor !== undefined
                && card.forecast.previousPeriodSpentMinor !== null
            text: card.previousPeriodText()
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
        case "exceeded": return "dialog-error-symbolic";
        case "critical": return "dialog-error-symbolic";
        case "warning": return "dialog-warning-symbolic";
        case "safe": return "dialog-ok-symbolic";
        default: return "question";
        }
    }

    function stateLabel() {
        switch (forecast.state) {
        case "exceeded": return i18n("Exceeded");
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
        if (forecast.kind === "budget_overrun" && forecast.state === "exceeded")
            return i18n("Reported spend has reached or exceeded the local policy limit.");
        if (forecast.predictedAt !== undefined
                && forecast.predictedAt !== null) {
            return forecast.kind === "budget_overrun"
                ? i18n("The configured budget is projected to be exceeded on %1.",
                       formatDate(forecast.predictedAt))
                : i18n("Quota is projected to be exhausted on %1, before reset.",
                       formatDate(forecast.predictedAt));
        }
        return forecast.kind === "budget_overrun"
            ? i18n("Projected period-end spend stays within the configured budget.")
            : i18n("No exhaustion is projected before this quota resets.");
    }

    function formatValue(value) {
        if (value === undefined || value === null) return "—";
        var number = Number(value);
        if (!Number.isFinite(number)) return "—";
        var formatted = number.toLocaleString(
            Qt.locale(), "f", forecast.currency ? currencyDigits() : 0);
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

    function formatMinor(minor) {
        var factor = minorToMajorFactor();
        if (!Number.isFinite(Number(minor)) || !Number.isFinite(factor))
            return "\u2014";
        return formatValue(Number(minor) * factor);
    }

    function previousPeriodText() {
        var spent = formatMinor(forecast.previousPeriodSpentMinor);
        if (forecast.previousPeriodChangePercent === undefined
                || forecast.previousPeriodChangePercent === null)
            return i18n("Previous period: %1", spent);
        return i18n("Previous period: %1 · change %2%", spent,
                    Number(forecast.previousPeriodChangePercent)
                        .toLocaleString(Qt.locale(), "f", 1));
    }

    function currencyDigits() {
        var factor = minorToMajorFactor();
        if (!Number.isFinite(factor) || factor <= 0) return 2;
        var ratio = 1 / factor;
        var digits = Math.round(Math.log(ratio) / Math.LN10);
        return digits >= 0 && digits <= 4 ? digits : 2;
    }

    function minorToMajorFactor() {
        var spentMinor = Number(forecast.spentMinor || 0);
        var spentMajor = Number(forecast.currentValue);
        if (spentMinor > 0 && Number.isFinite(spentMajor))
            return spentMajor / spentMinor;
        var limitMinor = spentMinor + Number(forecast.remainingMinor || 0);
        var limitMajor = Number(forecast.limitValue);
        return limitMinor > 0 && Number.isFinite(limitMajor)
            ? limitMajor / limitMinor : NaN;
    }

    function quotaMetric() {
        return String(forecast.window || "").indexOf("token") >= 0
            ? "tokens" : "requests";
    }
}
