pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: step
    required property var controller
    property string searchText: ""
    readonly property var filteredCandidates: filterCandidates()
    spacing: Kirigami.Units.mediumSpacing

    QQC2.ButtonGroup {
        id: sourceButtonGroup
        exclusive: true
    }
    property alias sourceSelectionGroup: sourceButtonGroup

    PlasmaExtras.Heading { level: 4; text: i18n("Choose a source") }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: i18n("Recommended sources appear first. The monitoring level tells you what the result can prove.")
    }

    Kirigami.SearchField {
        id: sourceSearch
        Layout.fillWidth: true
        placeholderText: i18n("Search sources")
        Accessible.name: i18n("Search monitoring sources")
        text: step.searchText
        onTextChanged: step.searchText = text
        activeFocusOnTab: true
    }

    ListView {
        id: sourceList
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(Kirigami.Units.gridUnit * 4,
            Math.min(contentHeight, Kirigami.Units.gridUnit * 14))
        Layout.minimumHeight: Math.min(contentHeight, Kirigami.Units.gridUnit * 4)
        clip: true
        spacing: Kirigami.Units.smallSpacing
        model: step.filteredCandidates
        Accessible.name: i18n("Available monitoring sources")

        delegate: ColumnLayout {
            id: sourceDelegate
            required property var modelData
            width: sourceList.width
            spacing: 0

            PlasmaComponents.RadioButton {
                Layout.fillWidth: true
                checked: step.controller.selectedSourceId === sourceDelegate.modelData.stableId
                QQC2.ButtonGroup.group: step.sourceSelectionGroup
                text: sourceDelegate.modelData.displayName + " — "
                      + step.controller.monitoringLevelLabel(sourceDelegate.modelData)
                Accessible.description: step.reportSummary(sourceDelegate.modelData)
                    + (sourceDelegate.modelData.nextActionText ? " " + sourceDelegate.modelData.nextActionText : "")
                onClicked: step.controller.selectSource(sourceDelegate.modelData.stableId, false)
                onToggled: if (checked && step.controller.selectedSourceId !== sourceDelegate.modelData.stableId)
                    step.controller.selectSource(sourceDelegate.modelData.stableId, false)
            }

            PlasmaComponents.Label {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                wrapMode: Text.WordWrap
                text: step.reportSummary(sourceDelegate.modelData)
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
            }
        }

    }

    PlasmaComponents.Label {
        visible: step.filteredCandidates.length === 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: step.controller.candidates.length === 0
            ? i18n("No matching source is available. Go back and choose another goal.")
            : i18n("No source matches your search.")
        color: Kirigami.Theme.disabledTextColor
    }

    RowLayout {
        Layout.fillWidth: true
        PlasmaComponents.Button { text: i18n("Back"); onClicked: step.controller.back() }
        Item { Layout.fillWidth: true }
        PlasmaComponents.Button {
            text: i18n("Continue")
            enabled: step.controller.selectedSourceId.length > 0
            onClicked: step.controller.selectSource(step.controller.selectedSourceId, true)
        }
    }

    function filterCandidates() {
        var query = searchText.trim().toLocaleLowerCase();
        var candidates = controller.candidates || [];
        if (!query) return candidates;
        return candidates.filter(function(candidate) {
            var description = controller.monitoringLevelLabel(candidate) || "";
            return [candidate.displayName, candidate.stableId, description,
                    reportSummary(candidate)]
                .join(" ").toLocaleLowerCase().indexOf(query) >= 0;
        });
    }

    function reportSummary(source) {
        var summaries = {
            "actual_usage_spend": i18n("Provider-reported usage, spend, or both when the endpoint supplies them."),
            "actual_key_usage": i18n("Usage attached to the configured API key."),
            "gateway_aggregate": i18n("Aggregate usage or spend reported by the gateway."),
            "balance_connectivity": i18n("Account balance and connection status; a balance is not spend history."),
            "connectivity_only": i18n("Credential or model access status; this source does not report usage or billing."),
            "actual_quota": i18n("Authenticated provider-reported quota and reset windows."),
            "local_activity_estimate": i18n("Local coding activity, shown as an estimate after activity is observed.")
        };
        return summaries[source.monitoringLevel] || i18n("The source's reported data appears after a successful check.");
    }

    function focusSearch() {
        sourceSearch.forceActiveFocus();
    }

    Component.onCompleted: Qt.callLater(focusSearch)
}
