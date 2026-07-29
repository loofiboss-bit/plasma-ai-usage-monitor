pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Rectangle {
    id: focus

    required property var presentation
    property var guardrails: null
    property bool forecastsEnabled: true
    readonly property var actionRow: presentation.topAction
    readonly property var runwayRisk: findTopRunwayRisk()
    readonly property bool showingRunway: !actionRow.stableId
        && !!runwayRisk.sourceId
    readonly property var effectiveActionRow: showingRunway ? {
        stableId: runwayRisk.sourceId,
        displayName: runwayRisk.sourceId,
        sourceKind: runwayRisk.sourceKind,
        nextActionKey: "review_runway",
        attentionSeverity: runwayRisk.state
    } : actionRow
    readonly property var facts: effectiveFacts()
    signal actionRequested(string stableId, string actionKey, string sourceKind)

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Qt.alpha(effectiveActionRow.attentionSeverity === "critical"
                    ? Kirigami.Theme.negativeBackgroundColor
                    : effectiveActionRow.attentionSeverity === "warning"
                      ? Kirigami.Theme.neutralBackgroundColor
                      : Kirigami.Theme.highlightColor, 0.12)
    border.width: 1
    border.color: Qt.alpha(effectiveActionRow.attentionSeverity === "critical"
                           ? Kirigami.Theme.negativeTextColor
                           : effectiveActionRow.attentionSeverity === "warning"
                             ? Kirigami.Theme.neutralTextColor
                             : Kirigami.Theme.highlightColor, 0.3)
    Accessible.role: Accessible.Grouping
    Accessible.name: headlineText() + ". " + explanationText()

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.mediumSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                PlasmaComponents.Label {
                    text: i18n("Daily focus")
                    color: Kirigami.Theme.disabledTextColor
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.capitalization: Font.AllUppercase
                }
                PlasmaExtras.Heading {
                    objectName: "dailyFocusHeadline"
                    Layout.fillWidth: true
                    level: 3
                    text: focus.headlineText()
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: focus.explanationText()
                    wrapMode: Text.WordWrap
                    color: focus.effectiveActionRow.attentionSeverity
                           && focus.effectiveActionRow.attentionSeverity !== "none"
                        ? Kirigami.Theme.neutralTextColor
                        : Kirigami.Theme.textColor
                }
            }

            PlasmaComponents.Button {
                objectName: "dailyFocusAction"
                visible: !!focus.effectiveActionRow.stableId
                    && focus.effectiveActionLabel() !== ""
                text: focus.effectiveActionLabel()
                icon.name: focus.showingRunway ? "chronometer"
                    : focus.presentation.actionIcon(
                        focus.effectiveActionRow)
                activeFocusOnTab: true
                Accessible.name: text + " · "
                    + (focus.effectiveActionRow.displayName || "")
                onClicked: focus.actionRequested(
                    focus.effectiveActionRow.stableId,
                    focus.effectiveActionRow.nextActionKey,
                    focus.effectiveActionRow.sourceKind)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: focus.width < Kirigami.Units.gridUnit * 22 ? 1 : 3
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            visible: focus.facts.length > 0

            Repeater {
                model: focus.facts

                RowLayout {
                    id: fact
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: fact.modelData.icon
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: width
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        PlasmaComponents.Label {
                            text: fact.modelData.value
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        PlasmaComponents.Label {
                            text: fact.modelData.label
                            color: Kirigami.Theme.disabledTextColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    function findTopRunwayRisk() {
        if (!forecastsEnabled || !guardrails) return ({});
        var rows = guardrails.forecasts || [];
        var top = null;
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || {};
            if (row.state !== "critical" && row.state !== "warning") continue;
            if (!top || row.state === "critical" && top.state !== "critical"
                    || row.state === top.state
                    && new Date(row.predictedAt).getTime()
                       < new Date(top.predictedAt).getTime()) {
                top = row;
            }
        }
        return top || ({});
    }

    function headlineText() {
        if (!showingRunway) return presentation.headline();
        return runwayRisk.kind === "budget_overrun"
            ? i18n("Monthly budget pacing needs attention")
            : i18n("Quota runway needs attention");
    }

    function explanationText() {
        if (!showingRunway) return presentation.explanation();
        var predicted = new Date(runwayRisk.predictedAt);
        if (runwayRisk.kind === "budget_overrun")
            return i18n("%1 is projected to exceed its configured budget on %2.",
                        runwayRisk.sourceId,
                        predicted.toLocaleString(Qt.locale()));
        return i18n("%1 is projected to exhaust quota before reset on %2.",
                    runwayRisk.sourceId,
                    predicted.toLocaleString(Qt.locale()));
    }

    function effectiveActionLabel() {
        return showingRunway ? i18n("Review runway")
                             : presentation.actionLabel(effectiveActionRow);
    }

    function effectiveFacts() {
        if (!showingRunway) return presentation.focusFacts();
        var result = [{
            icon: runwayRisk.state === "critical"
                ? "dialog-error-symbolic" : "dialog-warning-symbolic",
            value: runwayRisk.state === "critical"
                ? i18n("Critical") : i18n("Warning"),
            label: runwayRisk.kind === "budget_overrun"
                ? i18n("Budget pacing") : i18n("Quota runway")
        }, {
            icon: "chronometer",
            value: new Date(runwayRisk.predictedAt)
                .toLocaleString(Qt.locale()),
            label: i18n("Predicted event")
        }, {
            icon: "documentinfo",
            value: runwayRisk.evidenceGrade === "strong"
                ? i18n("Strong") : i18n("Usable"),
            label: i18n("%1 samples · %2% coverage",
                        Number(runwayRisk.sampleCount || 0),
                        Number(runwayRisk.coveragePercent || 0)
                            .toLocaleString(Qt.locale(), "f", 0))
        }];
        return result;
    }
}
